#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

class OrderBook;

class PriceCalculator {
public:
    PriceCalculator();
    ~PriceCalculator() = default;

    void registerOrderBook(const std::string& symbol, 
                          std::shared_ptr<OrderBook> order_book);

    std::optional<double> calculateImpliedUSDTPrice(
        const std::string& arb_pair,
        bool is_buy) const;

    std::optional<double> getDirectUSDTPrice(
        const std::string& pair,
        bool is_buy) const;

private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books_;

    std::optional<double> getPrice(
        const std::string& symbol,
        bool is_buy) const;

    std::optional<double> calculateViaBTC(
        const std::string& quote_currency,
        bool is_buy) const;

    std::optional<double> calculateViaETH(
        const std::string& quote_currency,
        bool is_buy) const;

    std::string extractQuoteCurrency(const std::string& pair) const;
};

