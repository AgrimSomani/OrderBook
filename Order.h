#pragma once

#include "Usings.h"
#include "OrderType.h"
#include "Side.h"
#include "Constants.h"

#include <list>
#include <exception>

class Order
{
public:
    Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) : orderType_{orderType}, orderId_{orderId}, side_{side}, price_{price}, initialQuantity_{quantity}, remainingQuantity_{quantity}
    {
    }

    // constructor for a market order
    Order(OrderId orderId, Side side, Quantity quantity) : Order(OrderType::Market, orderId, side, Constants::InvalidPrice, quantity)
    {
    }

    OrderId GetOrderId() const { return orderId_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    OrderType GetOrderType() const { return orderType_; }
    Quantity GetIntialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const { return initialQuantity_ - remainingQuantity_; }
    bool IsFilled() const { return GetRemainingQuantity() == 0; }
    void Fill(Quantity quantity)
    {
        if (quantity > GetRemainingQuantity())
        {
            throw std::logic_error("Order cannot be filled for more than its remaining quantity.");
        };
        remainingQuantity_ -= quantity;
    }
    void ToGoodTillCancel(Price price)
    {
        if (GetOrderType() != OrderType::Market)
            throw std::logic_error("Only market orders can have their price adjusted!");
        price_ = price;
        orderType_ = OrderType::GoodTillCancel;
    }

private:
    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;