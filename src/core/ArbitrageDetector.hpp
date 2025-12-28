#pragma once

#include "MarketState.hpp"
#include <string>
#include <optional>
#include <cmath>

struct ArbitrageOpportunity {
    int direction;  // 1 or 2
    std::string trade_sequence;
    std::string route_name;  // e.g., "ARB/BTC -> BTC/USDT"
    double profit_percent;
    
    // Prices for output (generalized - can be used for any route)
    double arb_usdt_bid;
    double arb_usdt_ask;
    double arb_other_bid;  // ARB/XXX bid
    double arb_other_ask;  // ARB/XXX ask
    double other_usdt_bid; // XXX/USDT bid
    double other_usdt_ask; // XXX/USDT ask
    
    bool valid;
    
    ArbitrageOpportunity()
        : direction(0), profit_percent(0.0),
          arb_usdt_bid(0.0), arb_usdt_ask(0.0),
          arb_other_bid(0.0), arb_other_ask(0.0),
          other_usdt_bid(0.0), other_usdt_ask(0.0),
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
    
    // Public methods to check individual routes (for UI display)
    std::optional<ArbitrageOpportunity> checkRoutePublic(
        const std::string& arb_pair,
        const std::string& cross_pair
    ) const;
    
    std::optional<ArbitrageOpportunity> checkDirectComparisonPublic(
        const std::string& arb_stable_pair
    ) const;
    
    // Public method to check multi-leg routes (for UI display)
    std::optional<ArbitrageOpportunity> checkMultiLegRoutePublic(
        const std::string& start_pair,  // e.g., "ARB/EUR"
        const std::string& intermediate_pair, // e.g., "ARB/BTC"
        const std::string& final_pair   // e.g., "BTC/USDT"
    ) const;

private:
    MarketState& market_state_;
    double threshold_percent_;
    mutable int check_count_;
    
    // Check all routes and return best opportunity
    std::optional<ArbitrageOpportunity> checkAllRoutes() const;
    
    // Check a specific route (e.g., ARB/BTC -> BTC/USDT)
    // Returns best opportunity from both directions
    std::optional<ArbitrageOpportunity> checkRoute(
        const std::string& arb_pair,  // e.g., "ARB/BTC"
        const std::string& cross_pair // e.g., "BTC/USDT"
    ) const;
    
    // Check direction 1 for a route: Buy implied, sell direct
    std::optional<ArbitrageOpportunity> checkRouteDirection1(
        const std::string& arb_pair,
        const std::string& cross_pair
    ) const;
    
    // Check direction 2 for a route: Buy direct, sell implied
    std::optional<ArbitrageOpportunity> checkRouteDirection2(
        const std::string& arb_pair,
        const std::string& cross_pair
    ) const;
    
    // Check direct comparison (for stablecoins: ARB/FDUSD, ARB/USDC, ARB/TUSD vs ARB/USDT)
    std::optional<ArbitrageOpportunity> checkDirectComparison(
        const std::string& arb_stable_pair  // e.g., "ARB/FDUSD"
    ) const;
    
    // Check multi-leg route (3+ legs)
    // Example: ARB/EUR -> ARB/BTC -> BTC/USDT
    std::optional<ArbitrageOpportunity> checkMultiLegRoute(
        const std::string& start_pair,      // e.g., "ARB/EUR"
        const std::string& intermediate_pair, // e.g., "ARB/BTC"
        const std::string& final_pair       // e.g., "BTC/USDT"
    ) const;
    
    // Validate price is reasonable (not zero, not NaN, not absurdly large)
    bool isValidPrice(double price) const;
    
    // Get snapshot for a symbol, return nullopt if invalid
    std::optional<OrderBook::Snapshot> getValidSnapshot(const std::string& symbol) const;
};
