#include "ArbitrageDetector.hpp"
#include <limits>
#include <algorithm>

ArbitrageDetector::ArbitrageDetector(MarketState& market_state, double threshold_percent)
    : market_state_(market_state),
      threshold_percent_(threshold_percent),
      check_count_(0) {}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkOpportunities() const {
    // Check both directions and return the one with higher profit (if above threshold)
    auto opp1 = checkDirection1();
    auto opp2 = checkDirection2();
    
    // Return the opportunity with higher profit if both are valid
    if (opp1.has_value() && opp2.has_value()) {
        if (opp1.value().profit_percent >= opp2.value().profit_percent) {
            return opp1;
        } else {
            return opp2;
        }
    }
    
    if (opp1.has_value()) {
        return opp1;
    }
    
    if (opp2.has_value()) {
        return opp2;
    }
    
    return std::nullopt;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkDirection1() const {
    // Direction 1: Buy implied, sell direct
    // cost_usdt  = ask(ARB/BTC) * ask(BTC/USDT)
    // final_usdt = bid(ARB/USDT)
    // profit%    = (final_usdt / cost_usdt - 1) * 100
    
    auto arb_btc_snap = getValidSnapshot("ARB/BTC");
    auto btc_usdt_snap = getValidSnapshot("BTC/USDT");
    auto arb_usdt_snap = getValidSnapshot("ARB/USDT");
    
    if (!arb_btc_snap.has_value() || !btc_usdt_snap.has_value() || !arb_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& arb_btc = arb_btc_snap.value();
    const auto& btc_usdt = btc_usdt_snap.value();
    const auto& arb_usdt = arb_usdt_snap.value();
    
    // Calculate cost and final
    double cost_usdt = arb_btc.ask_price * btc_usdt.ask_price;
    double final_usdt = arb_usdt.bid_price;
    
    // Validate calculations
    if (!isValidPrice(cost_usdt) || !isValidPrice(final_usdt) || cost_usdt <= 0.0) {
        return std::nullopt;
    }
    
    // Calculate profit percentage
    double profit_percent = (final_usdt / cost_usdt - 1.0) * 100.0;
    
    // Check threshold
    if (profit_percent < threshold_percent_) {
        return std::nullopt;
    }
    
    // Create opportunity
    ArbitrageOpportunity opp;
    opp.direction = 1;
    opp.trade_sequence = "Buy ARB/BTC -> Buy BTC/USDT -> Sell ARB/USDT";
    opp.profit_percent = profit_percent;
    opp.arb_usdt_bid = arb_usdt.bid_price;
    opp.arb_usdt_ask = arb_usdt.ask_price;
    opp.arb_btc_bid = arb_btc.bid_price;
    opp.arb_btc_ask = arb_btc.ask_price;
    opp.btc_usdt_bid = btc_usdt.bid_price;
    opp.btc_usdt_ask = btc_usdt.ask_price;
    opp.valid = true;
    
    return opp;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkDirection2() const {
    // Direction 2: Buy direct, sell implied
    // cost_usdt  = ask(ARB/USDT)
    // final_usdt = bid(ARB/BTC) * bid(BTC/USDT)
    // profit%    = (final_usdt / cost_usdt - 1) * 100
    
    auto arb_usdt_snap = getValidSnapshot("ARB/USDT");
    auto arb_btc_snap = getValidSnapshot("ARB/BTC");
    auto btc_usdt_snap = getValidSnapshot("BTC/USDT");
    
    if (!arb_usdt_snap.has_value() || !arb_btc_snap.has_value() || !btc_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& arb_usdt = arb_usdt_snap.value();
    const auto& arb_btc = arb_btc_snap.value();
    const auto& btc_usdt = btc_usdt_snap.value();
    
    // Calculate cost and final
    double cost_usdt = arb_usdt.ask_price;
    double final_usdt = arb_btc.bid_price * btc_usdt.bid_price;
    
    // Validate calculations
    if (!isValidPrice(cost_usdt) || !isValidPrice(final_usdt) || cost_usdt <= 0.0) {
        return std::nullopt;
    }
    
    // Calculate profit percentage
    double profit_percent = (final_usdt / cost_usdt - 1.0) * 100.0;
    
    // Check threshold
    if (profit_percent < threshold_percent_) {
        return std::nullopt;
    }
    
    // Create opportunity
    ArbitrageOpportunity opp;
    opp.direction = 2;
    opp.trade_sequence = "Buy ARB/USDT -> Sell ARB/BTC -> Sell BTC/USDT";
    opp.profit_percent = profit_percent;
    opp.arb_usdt_bid = arb_usdt.bid_price;
    opp.arb_usdt_ask = arb_usdt.ask_price;
    opp.arb_btc_bid = arb_btc.bid_price;
    opp.arb_btc_ask = arb_btc.ask_price;
    opp.btc_usdt_bid = btc_usdt.bid_price;
    opp.btc_usdt_ask = btc_usdt.ask_price;
    opp.valid = true;
    
    return opp;
}

bool ArbitrageDetector::isValidPrice(double price) const {
    // Check for NaN
    if (std::isnan(price)) {
        return false;
    }
    
    // Check for infinity
    if (std::isinf(price)) {
        return false;
    }
    
    // Check for zero or negative
    if (price <= 0.0) {
        return false;
    }
    
    // Check for absurdly large values (e.g., > 1 million)
    constexpr double MAX_REASONABLE_PRICE = 1000000.0;
    if (price > MAX_REASONABLE_PRICE) {
        return false;
    }
    
    return true;
}

std::optional<OrderBook::Snapshot> ArbitrageDetector::getValidSnapshot(const std::string& symbol) const {
    auto snap = market_state_.get(symbol).snapshot();
    
    if (!snap.has_data) {
        return std::nullopt;
    }
    
    // Validate bid and ask prices
    if (!isValidPrice(snap.bid_price) || !isValidPrice(snap.ask_price)) {
        return std::nullopt;
    }
    
    // Ensure bid <= ask (market sanity check)
    if (snap.bid_price > snap.ask_price) {
        return std::nullopt;
    }
    
    return snap;
}
