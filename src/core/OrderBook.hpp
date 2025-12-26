#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <limits>
#include <cstdint>
#include <chrono>

struct PriceLevel {
    double price;
    double quantity;

    PriceLevel(double p, double q) : price(p), quantity(q) {}
};

struct TopOfBook {
    double best_bid_price;
    double best_ask_price;
    double best_bid_qty;
    double best_ask_qty;
    uint64_t last_update_ms;
    
    TopOfBook() 
        : best_bid_price(0.0), best_ask_price(0.0),
          best_bid_qty(0.0), best_ask_qty(0.0),
          last_update_ms(0) {}
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

    // PDF gereksinimi: updateTop API
    void updateTop(
        double bid_price, double bid_qty,
        double ask_price, double ask_qty,
        uint64_t timestamp_ms);

    // PDF gereksinimi: getTop API (copy ile döndür)
    TopOfBook getTop() const;

    double getBestBid() const;
    double getBestAsk() const;
    double getBestBidQty() const;
    double getBestAskQty() const;
    uint64_t getLastUpdateMs() const;

    std::vector<PriceLevel> getTopNLevels(int n, bool is_bid) const;

    std::string getSymbol() const { return symbol_; }

    bool isValid() const;
    
    // Data freshness check
    bool isFresh(uint64_t max_age_ms = 500) const;

    double calculateTradableAmount(
        double max_amount,
        const std::vector<PriceLevel>& levels) const;

private:
    std::string symbol_;
    mutable std::mutex mutex_;
    
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    
    // PDF gereksinimi: Top of book data
    TopOfBook top_of_book_;

    void updateLevels(
        std::vector<PriceLevel>& existing,
        const std::vector<PriceLevel>& updates);

    void sortLevels(std::vector<PriceLevel>& levels, bool is_bid);
    
    void updateTopOfBook();
    
    static uint64_t getCurrentTimeMs();
};

