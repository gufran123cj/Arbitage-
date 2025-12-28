#include "OrderBook.hpp"

OrderBook::OrderBook()
    : bid_price_(0.0), bid_qty_(0.0), ask_price_(0.0), ask_qty_(0.0),
      timestamp_ms_(0), has_data_(false) {}

void OrderBook::update(double bid_price, double bid_qty, double ask_price, double ask_qty, int64_t timestamp_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    bid_price_ = bid_price;
    bid_qty_ = bid_qty;
    ask_price_ = ask_price;
    ask_qty_ = ask_qty;
    timestamp_ms_ = timestamp_ms;
    has_data_ = true;
}

OrderBook::Snapshot OrderBook::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Snapshot snap;
    snap.bid_price = bid_price_;
    snap.bid_qty = bid_qty_;
    snap.ask_price = ask_price_;
    snap.ask_qty = ask_qty_;
    snap.timestamp_ms = timestamp_ms_;
    snap.has_data = has_data_;
    return snap;
}
