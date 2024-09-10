#include "InputHandler.h"

#include <charconv>
#include <string_view>

std::uint32_t InputHandler::ToNumber(const std::string_view &str) const
{
    std::int64_t value{};
    // convert the string to the binary represenatation, with default base of 10
    std::from_chars(str.data(), str.data() + str.size(), value);
    if (value < 0)
    {
        throw std::logic_error("Value cant be below zero");
    }
    return static_cast<std::uint32_t>(value);
}

bool InputHandler::TryParseResult(const std::string_view &str, Result &result) const
{
    if (str.at(0) != 'R')
        return false;

    auto values = Split(str, ' ');
    result.allCount_ = ToNumber(values[1]);
    result.bidCount_ = ToNumber(values[2]);
    result.askCount_ = ToNumber(values[3]);

    return true;
}

bool InputHandler::TryParseInformation(const std::string_view &str, Information &information) const
{
    auto action = str.at(0);
    auto values = Split(str, ' ');

    // Add trade
    if (action == 'A')
    {
        information.type_ = ActionType::Add;
        information.side_ = ParseSide(values[1]);
        information.orderType_ = ParseOrderType(values[2]);
        information.price_ = ParsePrice(values[3]);
        information.quantity_ = ParseQuantity(values[4]);
        information.orderId_ = ParseOrderId(values[5]);
    }

    // Modify trade
    else if (action == 'M')
    {
        information.type_ = ActionType::Modify;
        information.orderId_ = ParseOrderId(values[1]);
        information.side_ = ParseSide(values[2]);
        information.price_ = ParsePrice(values[3]);
        information.quantity_ = ParseQuantity(values[4]);
    }

    // Cancel trade
    else if (action == 'C')
    {
        information.type_ = ActionType::Cancel;
        information.orderId_ = ParseOrderId(values[1]);
    }

    else
        return false;

    return true;
}

std::vector<std::string_view> InputHandler::Split(const std::string_view &str, const char &delimiter) const
{
    std::vector<std::string_view> res;
    // maximum splits are  in the test files
    res.reserve(6);
    std::size_t startIndex{}, endIndex{};

    while ((endIndex = str.find(delimiter, startIndex)) && endIndex != std::string::npos)
    {
        auto distance = endIndex - startIndex;
        auto toPush = str.substr(startIndex, distance);
        startIndex = endIndex + 1;
        res.push_back(toPush);
    }

    auto toPush = str.substr(startIndex);
    res.push_back(toPush);

    return res;
}

OrderType InputHandler::ParseOrderType(const std::string_view &str) const
{
    if (str == "Market")
        return OrderType::Market;
    else if (str == "FillAndKill")
        return OrderType::FillAndKill;
    else if (str == "FillOrKill")
        return OrderType::FillOrKill;
    else if (str == "GoodForDay")
        return OrderType::GoodForDay;
    else if (str == "GoodTillCancel")
        return OrderType::GoodTillCancel;
    else
        throw std::logic_error("Invalid Order Type");
}

Side InputHandler::ParseSide(const std::string_view &str) const
{
    if (str.at(0) == 'B')
    {
        return Side::Buy;
    }
    else if (str.at(0) == 'S')
    {
        return Side::Sell;
    }

    else
        throw std::logic_error("Invalid Side");
}

Price InputHandler::ParsePrice(const std::string_view &str) const
{
    if (str.empty())
        throw std::logic_error("Invalid Price");
    return ToNumber(str);
}

Quantity InputHandler::ParseQuantity(const std::string_view &str) const
{
    if (str.empty())
        throw std::logic_error("Invalid Quantity");
    return ToNumber(str);
}

OrderId InputHandler::ParseOrderId(const std::string_view &str) const
{
    if (str.empty())
        throw std::logic_error("Invalid Order Id");
    return static_cast<OrderId>(ToNumber(str));
}

std::tuple<Informations, Result> InputHandler::GetInformationsAndResult(const std::filesystem::path &path) const
{
    Informations informations;
    informations.reserve(1'000);

    std::string line;
    std::ifstream file{path};

    while (std::getline(file, line))
    {
        if (line.empty())
        {
            break;
        }

        const bool isResult = line.at(0) == 'R';
        const bool isInformation = !isResult;

        if (isInformation)
        {
            Information information;
            auto isValid = TryParseInformation(line, information);
            if (!isValid)
            {
                throw std::logic_error("One of the information line specified is invalid!");
            }
            informations.push_back(information);
        }

        else
        {
            if (!file.eof())
            {
                std::logic_error("The result line can only be at the end of the file");
            }
            Result result;
            auto isValid = TryParseResult(line, result);
            if (!isValid)
            {
                break;
            }
            return {informations, result};
        }
    }

    throw std::logic_error("Invalid Result Line");
};

// class OrderBookTextFixture : public googletest::TestWithParam<const char *>
// {
// private:
//     const static inline std::filesystem::path Root{std::filesystem::current_path()};
//     const static inline std::filesystem::path TestFolder{"TestFiles"};

// public:
//     const static inline std::filesystem::path TestFolderPath{Root / TestFolder};
// };

// TEST_P(OrderBookTextFixture, OrderbookTestSuite)
// {
//     // Arrange
//     const auto file = OrderBookTextFixture::TestFolderPath / GetParam();

//     InputHandler inputHandler;
//     const auto [informations, result] = inputHandler.GetInformationsAndResult(file);

//     auto GetOrder : [](const Information &information)
//     {
//         return std::make_shared<Order>(
//             information.orderType_,
//             information.orderId_,
//             information.side_,
//             information.price_,
//             information.quantity_);
//     };

//     auto GetModifyOrder : [](const Information &information)
//     {
//         return OrderModify(
//             information.orderId_,
//             information.side_,
//             information.price_,
//             information.quantity_);
//     };

//     // Act
//     OrderBook orderBook;

//     for (const auto &information : informations)
//     {
//         switch (information.type_)
//         {
//         case ActionType::Add:
//         {
//             const auto &trades = orderBook.AddOrder(GetOrder(information));
//         }
//         break;
//         case ActionType::Modify:
//         {
//             const auto &trades = orderBook.ModifyOrder(GetModifyOrder(information));
//         }
//         break;
//         case ActionType::Cancel:
//         {
//             orderBook.CancelOrder(information.orderId_);
//         }
//         break;
//         default:
//             throw std::logic_error("Unsupported Action");
//         }
//     }

//     // Assert
//     const auto &orderBookLevelInfos = orderBook.GetOrderInfos();
//     ASSERT_EQ(orderBook.size(), result.allCount_);
//     ASSERT_EQ(orderBookLevelInfos.GetAsks(), result.askCount_);
//     ASSERT_EQ(orderBookLevelInfos.GetBids(), result.bidCount_);
// }
