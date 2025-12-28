#include "MarketState.hpp"

OrderBook& MarketState::get(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(mutex_);
    return order_books_[symbol];
}

std::vector<std::string> MarketState::getSymbolsWithData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> symbols;
    for (const auto& pair : order_books_) {
        auto snap = pair.second.snapshot();
        if (snap.has_data) {
            symbols.push_back(pair.first);
        }
    }
    return symbols;
}
