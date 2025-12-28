#pragma once

#include "MarketState.hpp"
#include <string>
#include <optional>

struct PriceComparison {
    double direct_ask;
    double implied_ask;
    double difference_percent;
    bool valid;

    PriceComparison()
        : direct_ask(0.0), implied_ask(0.0), difference_percent(0.0), valid(false) {}
};

class PriceComparator {
public:
    explicit PriceComparator(MarketState& market_state);
    
    // Calculate implied ARB/USDT price and compare with direct
    // Returns optional because calculation may fail if data is missing
    std::optional<PriceComparison> compareArbUsdtPrices() const;

private:
    MarketState& market_state_;
    
    // Calculate implied ARB/USDT via ARB/BTC -> BTC/USDT
    std::optional<double> calculateImpliedArbUsdt() const;
    
    // Get direct ARB/USDT ask price
    std::optional<double> getDirectArbUsdtAsk() const;
    
    // Validate price is reasonable (not zero, not NaN, not absurdly large)
    bool isValidPrice(double price) const;
};
