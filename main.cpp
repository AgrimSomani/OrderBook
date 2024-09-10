#include "Orderbook.h"
#include "InputHandler.h"
#include <iostream>

std::shared_ptr<Order> GetOrder(const Information &information)
{
    return std::make_shared<Order>(
        information.orderType_,
        information.orderId_,
        information.side_,
        information.price_,
        information.quantity_);
}

OrderModify GetModifyOrder(const Information &information)
{
    return OrderModify(
        information.orderId_,
        information.side_,
        information.price_,
        information.quantity_);
}

int main()
{
    std::cout << "STARTED\n";

    // Parse with the input file containing instructions for the order book
    try
    {
        std::filesystem::path file{"Instructions.txt"};
        InputHandler inputHandler;
        const auto [informations, result] = inputHandler.GetInformationsAndResult(file);
        size_t informationSize = informations.size();
        std::cout << "PARSED INSTRUCTIONS\n";

        // Create the orderbook and execute those instructions in the file
        OrderBook orderBook;

        for (int i = 0; i < informationSize; i++)
        {
            auto information = informations.at(i);
            switch (information.type_)
            {
            case ActionType::Add:
            {
                const auto &trades = orderBook.AddOrder(GetOrder(information));
            }
            break;
            case ActionType::Modify:
            {
                const auto &trades = orderBook.ModifyOrder(GetModifyOrder(information));
            }
            break;
            case ActionType::Cancel:
            {
                orderBook.CancelOrder(information.orderId_);
            }
            break;
            default:
                throw std::logic_error("Unsupported Action");
            }

            std::cout << "\n=== Instruction " << i << " ===\n";
            std::cout << "----- Orderbook Summary -----\n";
            std::cout << "Orderbook Size: " << orderBook.Size() << "\n";
            std::cout << "Number of Ask Orders: " << orderBook.GetOrderInfos().GetAsks().size() << "\n";
            std::cout << "Number of Bid Orders: " << orderBook.GetOrderInfos().GetBids().size() << "\n";
            std::cout << "-------------------------------\n";
        }

        std::cout << "\nFINISHED";
        return 0;
    }
    catch (const std::logic_error &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

// OrderBook orderBook;
// orderBook.AddOrder(std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 10));
// std::cout << orderBook.Size() << "\n";
// orderBook.CancelOrder(1);
// std::cout << orderBook.Size();