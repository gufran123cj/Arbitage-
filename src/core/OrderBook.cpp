#include "OrderBook.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>

constexpr double EPSILON = 1e-9;

OrderBook::OrderBook(const std::string& symbol) : symbol_(symbol) {
    top_of_book_.last_update_ms = 0;
}

void OrderBook::applySnapshot(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    bids_ = bids;
    asks_ = asks;
    sortLevels(bids_, true);
    sortLevels(asks_, false);
    updateTopOfBook();
}

void OrderBook::applyUpdate(
    const std::vector<PriceLevel>& bids,
    const std::vector<PriceLevel>& asks) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    updateLevels(bids_, bids);
    updateLevels(asks_, asks);
    sortLevels(bids_, true);
    sortLevels(asks_, false);
    updateTopOfBook();
}

void OrderBook::updateTop(
    double bid_price, double bid_qty,
    double ask_price, double ask_qty,
    uint64_t timestamp_ms) {
    
    std::lock_guard<std::mutex> lock(mutex_);
    top_of_book_.best_bid_price = bid_price;
    top_of_book_.best_bid_qty = bid_qty;
    top_of_book_.best_ask_price = ask_price;
    top_of_book_.best_ask_qty = ask_qty;
    top_of_book_.last_update_ms = (timestamp_ms > 0) ? timestamp_ms : getCurrentTimeMs();
    
    // Also update vectors if needed
    if (!bids_.empty() && std::abs(bids_[0].price - bid_price) < EPSILON) {
        bids_[0].quantity = bid_qty;
    } else if (bid_price > 0.0) {
        bids_.insert(bids_.begin(), PriceLevel(bid_price, bid_qty));
        sortLevels(bids_, true);
    }
    
    if (!asks_.empty() && std::abs(asks_[0].price - ask_price) < EPSILON) {
        asks_[0].quantity = ask_qty;
    } else if (ask_price > 0.0) {
        asks_.insert(asks_.begin(), PriceLevel(ask_price, ask_qty));
        sortLevels(asks_, false);
    }
}

TopOfBook OrderBook::getTop() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return top_of_book_;
}

double OrderBook::getBestBidQty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) {
        return 0.0;
    }
    return bids_[0].quantity;
}

double OrderBook::getBestAskQty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) {
        return 0.0;
    }
    return asks_[0].quantity;
}

uint64_t OrderBook::getLastUpdateMs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return top_of_book_.last_update_ms;
}

bool OrderBook::isFresh(uint64_t max_age_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (top_of_book_.last_update_ms == 0) {
        return false;
    }
    uint64_t now_ms = getCurrentTimeMs();
    uint64_t age_ms = (now_ms > top_of_book_.last_update_ms) 
        ? (now_ms - top_of_book_.last_update_ms) 
        : 0;
    return age_ms <= max_age_ms;
}

void OrderBook::updateTopOfBook() {
    if (!bids_.empty() && !asks_.empty()) {
        top_of_book_.best_bid_price = bids_[0].price;
        top_of_book_.best_bid_qty = bids_[0].quantity;
        top_of_book_.best_ask_price = asks_[0].price;
        top_of_book_.best_ask_qty = asks_[0].quantity;
        top_of_book_.last_update_ms = getCurrentTimeMs();
    }
}

uint64_t OrderBook::getCurrentTimeMs() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
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

