#include "OrderBook.hpp"
#include <algorithm>
#include <cmath>

constexpr double EPSILON = 1e-9;

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {
}

void OrderBook::applySnapshot(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    bids_ = bids;
    asks_ = asks;
    sortLevels(bids_, true);
    sortLevels(asks_, false);
}

void OrderBook::applyUpdate(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    updateLevels(bids_, bids);
    updateLevels(asks_, asks);
    sortLevels(bids_, true);
    sortLevels(asks_, false);
}

double OrderBook::getBestBid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) {
        return 0.0;
    }
    return bids_[0].price;
}

double OrderBook::getBestAsk() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) {
        return 0.0;
    }
    return asks_[0].price;
}

std::vector<PriceLevel> OrderBook::getTopNLevels(int n, bool is_bid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    const auto& levels = is_bid ? bids_ : asks_;
    int count = std::min(n, static_cast<int>(levels.size()));
    
    std::vector<PriceLevel> result;
    result.reserve(count);
    
    for (int i = 0; i < count; ++i) {
        result.push_back(levels[i]);
    }
    
    return result;
}

bool OrderBook::isValid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty() || asks_.empty()) {
        return false;
    }
    // Direct access instead of calling getBestBid/getBestAsk to avoid deadlock
    return bids_[0].price > 0.0 && asks_[0].price > 0.0;
}

double OrderBook::calculateTradableAmount(
    double max_amount,
    const std::vector<PriceLevel>& levels) const {
    
    double remaining = max_amount;
    double total_cost = 0.0;
    
    for (const auto& level : levels) {
        if (remaining <= EPSILON) {
            break;
        }
        
        double quantity_to_take = std::min(remaining, level.quantity);
        total_cost += quantity_to_take * level.price;
        remaining -= quantity_to_take;
    }
    
    return total_cost;
}

void OrderBook::updateLevels(
    std::vector<PriceLevel>& existing,
    const std::vector<PriceLevel>& updates) {
    
    for (const auto& update : updates) {
        if (std::abs(update.quantity) < EPSILON) {
            existing.erase(
                std::remove_if(existing.begin(), existing.end(),
                    [&update](const PriceLevel& level) {
                        return std::abs(level.price - update.price) < EPSILON;
                    }),
                existing.end());
        } else {
            auto it = std::find_if(existing.begin(), existing.end(),
                [&update](const PriceLevel& level) {
                    return std::abs(level.price - update.price) < EPSILON;
                });
            
            if (it != existing.end()) {
                it->quantity = update.quantity;
            } else {
                existing.push_back(update);
            }
        }
    }
}

void OrderBook::sortLevels(std::vector<PriceLevel>& levels, bool is_bid) {
    if (is_bid) {
        std::sort(levels.begin(), levels.end(),
            [](const PriceLevel& a, const PriceLevel& b) {
                return a.price > b.price;
            });
    } else {
        std::sort(levels.begin(), levels.end(),
            [](const PriceLevel& a, const PriceLevel& b) {
                return a.price < b.price;
            });
    }
}

