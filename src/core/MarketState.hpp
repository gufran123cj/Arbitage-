#pragma once

#include "OrderBook.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <vector>

class MarketState {
public:
    // Thread-safe access to OrderBook
    OrderBook& get(const std::string& symbol);
    
    // Get all symbols that have data
    std::vector<std::string> getSymbolsWithData() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, OrderBook> order_books_;
};
