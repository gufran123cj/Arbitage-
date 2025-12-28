#pragma once

#include "MarketState.hpp"
#include <string>
#include <optional>
#include <cmath>

struct ArbitrageOpportunity {
    int direction;  // 1 or 2
    std::string trade_sequence;
    double profit_percent;
    
    // Prices for output
    double arb_usdt_bid;
    double arb_usdt_ask;
    double arb_btc_bid;
    double arb_btc_ask;
    double btc_usdt_bid;
    double btc_usdt_ask;
    
    bool valid;
    
    ArbitrageOpportunity()
        : direction(0), profit_percent(0.0),
          arb_usdt_bid(0.0), arb_usdt_ask(0.0),
          arb_btc_bid(0.0), arb_btc_ask(0.0),
          btc_usdt_bid(0.0), btc_usdt_ask(0.0),
          valid(false) {}
};

class ArbitrageDetector {
public:
    explicit ArbitrageDetector(MarketState& market_state, double threshold_percent = 0.10);
    
    // Check for arbitrage opportunities
    // Returns optional because check may fail if data is missing
    std::optional<ArbitrageOpportunity> checkOpportunities() const;
    
    // Get current check count (for heartbeat)
    int getCheckCount() const { return check_count_; }
    
    // Increment check count (call after each check)
    void incrementCheckCount() { ++check_count_; }
    
    // Reset check count (for heartbeat)
    void resetCheckCount() { check_count_ = 0; }

private:
    MarketState& market_state_;
    double threshold_percent_;
    mutable int check_count_;
    
    // Direction 1: Buy implied (ARB/BTC -> BTC/USDT), sell direct (ARB/USDT)
    std::optional<ArbitrageOpportunity> checkDirection1() const;
    
    // Direction 2: Buy direct (ARB/USDT), sell implied (ARB/BTC -> BTC/USDT)
    std::optional<ArbitrageOpportunity> checkDirection2() const;
    
    // Validate price is reasonable (not zero, not NaN, not absurdly large)
    bool isValidPrice(double price) const;
    
    // Get snapshot for a symbol, return nullopt if invalid
    std::optional<OrderBook::Snapshot> getValidSnapshot(const std::string& symbol) const;
};
