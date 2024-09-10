#pragma once

#include <map>
#include <unordered_map>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

#include "Usings.h"
#include "Order.h"
#include "OrderModify.h"
#include "OrderbookLevelInfos.h"
#include "Trade.h"

using OrderIds = std::vector<OrderId>;

class OrderBook
{
private:
    struct OrderEntry
    {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    struct LevelData
    {
        Quantity askQuantity_{};
        Quantity bidQuantity_{};
        Quantity count_{}; // number of orders for this price level, used to remove the price level from map if no orders

        enum class Action
        {
            Add,
            Remove,
            Match
        };
    };

    std::unordered_map<Price, LevelData> data_;
    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    // these data structures are for the pruning thread and avoiding race conditions
    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{false};

    void PruneGoodForDayOrders();

    bool CanMatch(Side side, Price price) const;
    Trades MatchOrder();
    bool CanFullyFill(Side side, Price price, Quantity quantity) const;

    // these methods are for maintaining the metadata for each price level in the orderbook
    void OnOrderAdded(OrderPointer order);
    void OnOrderRemoved(OrderPointer order);
    void OnOrderMatched(Side side, Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Side side, Price price, Quantity quantity, LevelData::Action action);

    void CancelOrders(OrderIds orderIds);
    void CancelOrderInternal(OrderId orderId);

public:
    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderId orderId);
    Trades ModifyOrder(OrderModify orderModify);

    std::size_t Size() const;
    OrderbookLevelInfos GetOrderInfos() const;
};
