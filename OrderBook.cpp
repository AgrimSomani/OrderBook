#include "Orderbook.h"

#include <numeric>
#include <chrono>
#include <ctime>
#include <mutex>
#include <optional>
#include <iostream>

void OrderBook::PruneGoodForDayOrders()
{
    using namespace std::chrono;
    const auto end = hours(16);
    while (true)
    {
        const auto now = system_clock::now();
        // convert the time to a time_t value, representing the seconds since the epoch
        const auto now_c = system_clock::to_time_t(now);
        std::tm now_parts;
        // breaks the time_t into a std::tm structure, which represents the data and time in year, month, day , hour, minute, second
        localtime_s(&now_parts, &now_c);

        if (now_parts.tm_hour >= end.count())
        {
            // Already past 4pm today, set the target time for 4pm tomorrow
            now_parts.tm_mday += 1;
        }

        // Get the exact time for 4PM
        now_parts.tm_hour = end.count();
        now_parts.tm_min = 0;
        now_parts.tm_sec = 0;

        // Convert the std::tm back to a time_point
        auto next = system_clock::from_time_t(mktime(&now_parts));
        auto till = next - now + milliseconds(100);

        {
            std::unique_lock<std::mutex> ordersLock{ordersMutex_};

            if (shutdown_.load(std::memory_order_acquire) || shutdownConditionVariable_.wait_for(ordersLock, till) == std::cv_status::no_timeout)
                return;
        }

        OrderIds orderIds;
        // get the mutex and then get all the good for day trades, and call cancel orders on them
        {
            std::scoped_lock ordersLock{ordersMutex_};

            for (const auto &[orderId, orderEntry] : orders_)
            {
                const auto &[order, _] = orderEntry;
                if (order->GetOrderType() != OrderType::GoodForDay)
                {
                    continue;
                }
                orderIds.push_back(order->GetOrderId());
            }
        }

        CancelOrders(orderIds);
    }
};

void OrderBook::CancelOrders(OrderIds orderIds)
{
    std::scoped_lock ordersLock{ordersMutex_};

    for (const auto &orderId : orderIds)
    {
        CancelOrderInternal(orderId);
    }
}

void OrderBook::CancelOrderInternal(OrderId orderId)
{
    if (orders_.count(orderId) == 0)
        return;

    // get the order from the map, and use the iterator to remove the order double linked list from the bid/ask map at the order price level
    const auto [order, orderIterator] = orders_[orderId];
    orders_.erase(orderId);

    // remove the order from the bid map
    if (order->GetSide() == Side::Buy)
    {
        auto price = order->GetPrice();
        auto &orders = bids_.at(price);
        orders.erase(orderIterator);
        if (orders.empty())
        {
            bids_.erase(price);
        }
    }
    // remove the order from the ask map
    else
    {
        auto price = order->GetPrice();
        auto &orders = asks_.at(price);
        orders.erase(orderIterator);
        if (orders.empty())
        {
            asks_.erase(price);
        }
    }

    OnOrderRemoved(order);
}

void OrderBook::OnOrderAdded(OrderPointer order)
{
    UpdateLevelData(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Add);
};

void OrderBook::OnOrderRemoved(OrderPointer order)
{
    UpdateLevelData(order->GetSide(), order->GetPrice(), order->GetRemainingQuantity(), LevelData::Action::Remove);
};

void OrderBook::OnOrderMatched(Side side, Price price, Quantity quantity, bool isFullyFilled)
{
    UpdateLevelData(side, price, quantity, isFullyFilled ? LevelData::Action::Remove : LevelData::Action::Match);
};

void OrderBook::UpdateLevelData(Side side, Price price, Quantity quantity, LevelData::Action action)
{
    auto &levelData = data_[price];

    levelData.count_ += action == LevelData::Action::Add ? 1 : action == LevelData::Action::Remove ? -1
                                                                                                   : 0;

    if (action == LevelData::Action::Add)
    {
        if (side == Side::Buy)
            levelData.bidQuantity_ += quantity;
        else
            levelData.askQuantity_ += quantity;
    }
    else
    {
        if (side == Side::Buy)
            levelData.bidQuantity_ -= quantity;
        else
            levelData.askQuantity_ -= quantity;
    }

    if (levelData.count_ == 0)
    {
        data_.erase(price);
    }
}

bool OrderBook::CanMatch(Side side, Price price) const
{
    if (side == Side::Buy)
    {
        if (asks_.empty())
        {
            return false;
        }

        const auto &[bestAsk, _] = *asks_.begin();
        return price >= bestAsk;
    }
    else
    {
        if (bids_.empty())
        {
            return false;
        }
        const auto &[bestBid, _] = *bids_.begin();
        return price <= bestBid;
    }
}

Trades OrderBook::MatchOrder()
{
    Trades trades;
    trades.reserve(orders_.size());

    while (true)
    {
        // no more bid or ask orders left
        if (bids_.empty() || asks_.empty())
            break;

        // best bid and best ask orders
        auto &[bidPrice, bids] = *bids_.begin();
        auto &[askPrice, asks] = *asks_.begin();

        if (bidPrice < askPrice)
            break;

        // match orders to create trades
        while (!bids.empty() && !asks.empty())
        {
            // get the orders based on when submitted, lowest to highest
            auto bid = bids.front();
            auto ask = asks.front();

            // match these orders for max amount of quantity
            Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

            bid->Fill(quantity);
            ask->Fill(quantity);

            // remove the orders if they are completely filled
            if (bid->IsFilled())
            {
                bids.pop_front();
                orders_.erase(bid->GetOrderId());
            };

            if (ask->IsFilled())
            {
                asks.pop_front();
                orders_.erase(ask->GetOrderId());
            };

            // create the trade
            trades.push_back(Trade{TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity}, TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity}});

            // call for bid and ask order
            OnOrderMatched(Side::Buy, bid->GetPrice(), quantity, bid->IsFilled());
            OnOrderMatched(Side::Sell, ask->GetPrice(), quantity, ask->IsFilled());
        }

        // Remove the price level of a bid/ask if all the orders in this price level were matched, from the bids and asks map
        if (bids.empty())
        {
            bids_.erase(bidPrice);
        }
        if (asks.empty())
        {
            asks_.erase(askPrice);
        }

        // Remove the price level from the data map if BOTH the bid and ask quantity for the price level is 0
        if (data_.contains(bidPrice) && data_.at(bidPrice).bidQuantity_ == 0 && data_.at(bidPrice).askQuantity_ == 0)
        {
            data_.erase(bidPrice);
        }
        if (data_.contains(askPrice) && data_.at(askPrice).bidQuantity_ == 0 && data_.at(askPrice).askQuantity_ == 0)
        {
            data_.erase(askPrice);
        }

        // Cancel any bid/ask orders which were fill and kill type
        if (!bids_.empty())
        {

            auto &[_, bids] = *bids_.begin();
            auto &order = *bids.front();

            if (order.GetOrderType() == OrderType::FillAndKill)
            {
                CancelOrder(order.GetOrderId());
            }
        }

        if (!asks_.empty())
        {
            auto &[_, asks] = *asks_.begin();
            auto &order = *asks.front();

            if (order.GetOrderType() == OrderType::FillAndKill)
            {
                CancelOrder(order.GetOrderId());
            }
        }
    }

    return trades;
}

bool OrderBook::CanFullyFill(Side side, Price price, Quantity quantity) const
{
    if (!CanMatch(side, price))
        return false;

    std::optional<Price> threshold;

    // get the best bid or best ask from the asks or bids map
    if (side == Side::Buy)
    {
        const auto &[askPrice, _] = *asks_.begin();
        threshold = askPrice;
    }
    else
    {
        const auto &[bidPrice, _] = *bids_.begin();
        threshold = bidPrice;
    }

    for (const auto &[levelPrice, levelData] : data_)
    {
        // safe keeping to make sure the best bid and best ask incase the best bid and best ask in data map is outdated
        if (threshold.has_value() && ((side == Side::Buy && levelPrice < threshold.value()) || (side == Side::Sell && levelPrice > threshold.value())))
            continue;

        // If the existing orders cant match the given orders price, continue
        if ((side == Side::Buy && levelPrice > price) || (side == Side::Sell && levelPrice < price))
            continue;

        // If there are no orders of the opposite side
        if ((side == Side::Buy && levelData.askQuantity_ == 0) || (side == Side::Sell && levelData.bidQuantity_ == 0))
        {
            continue;
        }

        if ((side == Side::Buy && levelData.askQuantity_ >= quantity) || (side == Side::Sell && levelData.bidQuantity_ >= quantity))
            return true;

        quantity -= side == Side::Buy ? levelData.askQuantity_ : levelData.bidQuantity_;
    }

    return false;
};

Trades OrderBook::AddOrder(OrderPointer order)
{

    std::scoped_lock ordersLock{ordersMutex_};

    // order already exists
    if (orders_.contains(order->GetOrderId()))
    {
        return {};
    }

    if (order->GetOrderType() == OrderType::Market)
    {
        // convert the market order to a limit order with with worst bid/ask in the orderbook
        if (order->GetSide() == Side::Buy && !asks_.empty())
        {
            const auto &[worstAsk, _] = *asks_.rbegin();
            order->ToGoodTillCancel(worstAsk);
        }
        else if (order->GetSide() == Side::Sell && !bids_.empty())
        {
            const auto &[worstBid, _] = *bids_.rbegin();
            order->ToGoodTillCancel(worstBid);
        }
        else
        {
            return {};
        }
    }

    // if order is of type fill and kill and it cant match with any other orders, then discard order right there and then
    if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
    {
        return {};
    }

    // if fill or kill order but cant fully fill, then dont add the order in the orderbook
    if (order->GetOrderType() == OrderType::FillOrKill && !CanFullyFill(order->GetSide(), order->GetPrice(), order->GetIntialQuantity()))
    {
        return {};
    }

    OrderPointers::iterator iterator;

    // add the order to the corresponding dict
    if (order->GetSide() == Side::Buy)
    {
        auto &orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    }
    else
    {
        auto &orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iterator = std::next(orders.begin(), orders.size() - 1);
    };

    // add the order to the cumalative order list
    orders_.insert({order->GetOrderId(), OrderEntry{order, iterator}});

    OnOrderAdded(order);

    return MatchOrder();
}

Trades OrderBook::ModifyOrder(OrderModify orderModify)
{
    OrderType orderType;
    {
        std::scoped_lock ordersLock{ordersMutex_};

        // this order id is not in orders map
        if (!orders_.contains(orderModify.GetOrderId()))
        {
            return {};
        }

        // get the old order, and save the order type to add to the new modified order
        const auto &[order, iterator] = orders_.at(orderModify.GetOrderId());
        orderType = order->GetOrderType();
    }

    CancelOrder(orderModify.GetOrderId());
    return AddOrder(orderModify.ToOrderPointer(orderType));
}

void OrderBook::CancelOrder(OrderId orderId)
{
    std::scoped_lock ordersLock{ordersMutex_};
    CancelOrderInternal(orderId);
};

std::size_t OrderBook::Size() const
{
    std::scoped_lock ordersLock{ordersMutex_};
    return orders_.size();
}

OrderbookLevelInfos OrderBook::GetOrderInfos() const
{
    LevelInfos bidInfos, askInfos;
    bidInfos.reserve(orders_.size());
    askInfos.reserve(orders_.size());

    // lambda function which returns the total quantity ordered from each price level in the bid/ask map
    auto CreateLevelInfos = [](Price price, const OrderPointers &orders)
    {
        return LevelInfo{
            price, std::accumulate(orders.begin(), orders.end(), (Quantity)0, [](Quantity runningSum, const OrderPointer &order)
                                   { return runningSum + order->GetRemainingQuantity(); })};
    };

    for (const auto &[price, orders] : bids_)
    {
        bidInfos.push_back(CreateLevelInfos(price, orders));
    };

    for (const auto &[price, orders] : asks_)
    {
        askInfos.push_back(CreateLevelInfos(price, orders));
    };

    return OrderbookLevelInfos{bidInfos, askInfos};
}
