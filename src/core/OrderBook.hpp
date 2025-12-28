#pragma once

#include <mutex>
#include <chrono>

class OrderBook {
public:
    struct Snapshot {
        double bid_price;
        double bid_qty;
        double ask_price;
        double ask_qty;
        int64_t timestamp_ms;
        bool has_data;

        Snapshot()
            : bid_price(0.0), bid_qty(0.0), ask_price(0.0), ask_qty(0.0),
              timestamp_ms(0), has_data(false) {}
    };

    OrderBook();
    
    // Thread-safe update
    void update(double bid_price, double bid_qty, double ask_price, double ask_qty, int64_t timestamp_ms);
    
    // Thread-safe snapshot
    Snapshot snapshot() const;

private:
    mutable std::mutex mutex_;
    double bid_price_;
    double bid_qty_;
    double ask_price_;
    double ask_qty_;
    int64_t timestamp_ms_;
    bool has_data_;
};
