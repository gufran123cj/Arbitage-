#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <limits>

struct PriceLevel {
    double price;
    double quantity;

    PriceLevel(double p, double q) : price(p), quantity(q) {}
};

class OrderBook {
public:
    OrderBook(const std::string& symbol);
    ~OrderBook() = default;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    void applySnapshot(
        const std::vector<PriceLevel>& bids,
        const std::vector<PriceLevel>& asks);

    void applyUpdate(
        const std::vector<PriceLevel>& bids,
        const std::vector<PriceLevel>& asks);

    double getBestBid() const;
    double getBestAsk() const;

    std::vector<PriceLevel> getTopNLevels(int n, bool is_bid) const;

    std::string getSymbol() const { return symbol_; }

    bool isValid() const;

    double calculateTradableAmount(
        double max_amount,
        const std::vector<PriceLevel>& levels) const;

private:
    std::string symbol_;
    mutable std::mutex mutex_;
    
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;

    void updateLevels(
        std::vector<PriceLevel>& existing,
        const std::vector<PriceLevel>& updates);

    void sortLevels(std::vector<PriceLevel>& levels, bool is_bid);
};

