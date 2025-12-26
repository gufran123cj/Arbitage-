#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "OrderBook.hpp"

struct MarketUpdate {
    std::string symbol;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    bool is_snapshot;
    uint64_t timestamp_ms;  // PDF gereksinimi: last_update_ms
    
    MarketUpdate() : is_snapshot(false), timestamp_ms(0) {}
};

