#pragma once

#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <string_view>

#include "OrderBook.h"

enum class ActionType
{
    Add,
    Modify,
    Cancel
};

struct Information
{
    ActionType type_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity quantity_;
    OrderId orderId_;
};

using Informations = std::vector<Information>;

struct Result
{
    std::size_t allCount_;
    std::size_t bidCount_;
    std::size_t askCount_;
};

struct InputHandler
{
private:
    std::uint32_t ToNumber(const std::string_view &str) const;
    bool TryParseResult(const std::string_view &str, Result &result) const;
    bool TryParseInformation(const std::string_view &str, Information &information) const;
    std::vector<std::string_view> Split(const std::string_view &str, const char &delimiter) const;
    OrderType ParseOrderType(const std::string_view &str) const;
    Side ParseSide(const std::string_view &str) const;
    Price ParsePrice(const std::string_view &str) const;
    Quantity ParseQuantity(const std::string_view &str) const;
    OrderId ParseOrderId(const std::string_view &str) const;

public:
    std::tuple<Informations, Result> GetInformationsAndResult(const std::filesystem::path &path) const;
};
