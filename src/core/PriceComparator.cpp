#include "PriceComparator.hpp"
#include <cmath>
#include <limits>

PriceComparator::PriceComparator(MarketState& market_state)
    : market_state_(market_state) {}

std::optional<PriceComparison> PriceComparator::compareArbUsdtPrices() const {
    PriceComparison result;
    
    // Get direct ARB/USDT ask price
    auto direct_ask_opt = getDirectArbUsdtAsk();
    if (!direct_ask_opt.has_value()) {
        return std::nullopt; // Missing direct price
    }
    result.direct_ask = direct_ask_opt.value();
    
    // Calculate implied ARB/USDT price
    auto implied_ask_opt = calculateImpliedArbUsdt();
    if (!implied_ask_opt.has_value()) {
        return std::nullopt; // Missing data for implied calculation
    }
    result.implied_ask = implied_ask_opt.value();
    
    // Validate both prices are reasonable
    if (!isValidPrice(result.direct_ask) || !isValidPrice(result.implied_ask)) {
        return std::nullopt; // Invalid prices
    }
    
    // Calculate percentage difference
    // diff_percent = (implied / direct - 1.0) * 100.0
    if (result.direct_ask > 0.0) {
        result.difference_percent = (result.implied_ask / result.direct_ask - 1.0) * 100.0;
    } else {
        return std::nullopt; // Division by zero protection
    }
    
    result.valid = true;
    return result;
}

std::optional<double> PriceComparator::calculateImpliedArbUsdt() const {
    // Get ARB/BTC snapshot
    auto arb_btc_snap = market_state_.get("ARB/BTC").snapshot();
    if (!arb_btc_snap.has_data || !isValidPrice(arb_btc_snap.ask_price)) {
        return std::nullopt;
    }
    
    // Get BTC/USDT snapshot
    auto btc_usdt_snap = market_state_.get("BTC/USDT").snapshot();
    if (!btc_usdt_snap.has_data || !isValidPrice(btc_usdt_snap.ask_price)) {
        return std::nullopt;
    }
    
    // Calculate implied: ask(ARB/BTC) * ask(BTC/USDT)
    double implied = arb_btc_snap.ask_price * btc_usdt_snap.ask_price;
    
    if (!isValidPrice(implied)) {
        return std::nullopt;
    }
    
    return implied;
}

std::optional<double> PriceComparator::getDirectArbUsdtAsk() const {
    auto snap = market_state_.get("ARB/USDT").snapshot();
    
    if (!snap.has_data || !isValidPrice(snap.ask_price)) {
        return std::nullopt;
    }
    
    return snap.ask_price;
}

bool PriceComparator::isValidPrice(double price) const {
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
    // This is a sanity check - BTC/USDT is around 60k-100k, so 1M is a reasonable upper bound
    constexpr double MAX_REASONABLE_PRICE = 1000000.0;
    if (price > MAX_REASONABLE_PRICE) {
        return false;
    }
    
    return true;
}
