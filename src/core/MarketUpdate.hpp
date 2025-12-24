#pragma once

#include <string>
#include <vector>
#include "OrderBook.hpp"

struct MarketUpdate {
    std::string symbol;
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    bool is_snapshot;
};

