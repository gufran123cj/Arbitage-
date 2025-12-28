#include "ArbitrageDetector.hpp"
#include <limits>
#include <algorithm>

ArbitrageDetector::ArbitrageDetector(MarketState& market_state, double threshold_percent)
    : market_state_(market_state),
      threshold_percent_(threshold_percent),
      check_count_(0) {}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkOpportunities() const {
    return checkAllRoutes();
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkAllRoutes() const {
    std::optional<ArbitrageOpportunity> best_opp;
    double best_profit = -1.0;
    
    // Check all cross-pair routes
    // ARB/BTC -> BTC/USDT
    auto opp_btc = checkRoute("ARB/BTC", "BTC/USDT");
    if (opp_btc.has_value() && opp_btc.value().profit_percent > best_profit) {
        best_opp = opp_btc;
        best_profit = opp_btc.value().profit_percent;
    }
    
    // ARB/ETH -> ETH/USDT
    auto opp_eth = checkRoute("ARB/ETH", "ETH/USDT");
    if (opp_eth.has_value() && opp_eth.value().profit_percent > best_profit) {
        best_opp = opp_eth;
        best_profit = opp_eth.value().profit_percent;
    }
    
    // ARB/EUR -> EUR/USDT
    auto opp_eur = checkRoute("ARB/EUR", "EUR/USDT");
    if (opp_eur.has_value() && opp_eur.value().profit_percent > best_profit) {
        best_opp = opp_eur;
        best_profit = opp_eur.value().profit_percent;
    }
    
    // ARB/TRY -> TRY/USDT
    auto opp_try = checkRoute("ARB/TRY", "TRY/USDT");
    if (opp_try.has_value() && opp_try.value().profit_percent > best_profit) {
        best_opp = opp_try;
        best_profit = opp_try.value().profit_percent;
    }
    
    // Check direct comparisons for stablecoins
    auto opp_fdusd = checkDirectComparison("ARB/FDUSD");
    if (opp_fdusd.has_value() && opp_fdusd.value().profit_percent > best_profit) {
        best_opp = opp_fdusd;
        best_profit = opp_fdusd.value().profit_percent;
    }
    
    auto opp_usdc = checkDirectComparison("ARB/USDC");
    if (opp_usdc.has_value() && opp_usdc.value().profit_percent > best_profit) {
        best_opp = opp_usdc;
        best_profit = opp_usdc.value().profit_percent;
    }
    
    auto opp_tusd = checkDirectComparison("ARB/TUSD");
    if (opp_tusd.has_value() && opp_tusd.value().profit_percent > best_profit) {
        best_opp = opp_tusd;
        best_profit = opp_tusd.value().profit_percent;
    }
    
    // Check multi-leg routes (3+ legs)
    // ARB/EUR -> ARB/BTC -> BTC/USDT
    auto opp_eur_btc = checkMultiLegRoute("ARB/EUR", "ARB/BTC", "BTC/USDT");
    if (opp_eur_btc.has_value() && opp_eur_btc.value().profit_percent > best_profit) {
        best_opp = opp_eur_btc;
        best_profit = opp_eur_btc.value().profit_percent;
    }
    
    // ARB/EUR -> ARB/ETH -> ETH/USDT
    auto opp_eur_eth = checkMultiLegRoute("ARB/EUR", "ARB/ETH", "ETH/USDT");
    if (opp_eur_eth.has_value() && opp_eur_eth.value().profit_percent > best_profit) {
        best_opp = opp_eur_eth;
        best_profit = opp_eur_eth.value().profit_percent;
    }
    
    // ARB/TRY -> ARB/BTC -> BTC/USDT
    auto opp_try_btc = checkMultiLegRoute("ARB/TRY", "ARB/BTC", "BTC/USDT");
    if (opp_try_btc.has_value() && opp_try_btc.value().profit_percent > best_profit) {
        best_opp = opp_try_btc;
        best_profit = opp_try_btc.value().profit_percent;
    }
    
    // ARB/TRY -> ARB/ETH -> ETH/USDT
    auto opp_try_eth = checkMultiLegRoute("ARB/TRY", "ARB/ETH", "ETH/USDT");
    if (opp_try_eth.has_value() && opp_try_eth.value().profit_percent > best_profit) {
        best_opp = opp_try_eth;
        best_profit = opp_try_eth.value().profit_percent;
    }
    
    return best_opp;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkRoute(
    const std::string& arb_pair,
    const std::string& cross_pair
) const {
    auto opp1 = checkRouteDirection1(arb_pair, cross_pair);
    auto opp2 = checkRouteDirection2(arb_pair, cross_pair);
    
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

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkRouteDirection1(
    const std::string& arb_pair,
    const std::string& cross_pair
) const {
    // Direction 1: Buy implied, sell direct
    // cost_usdt  = ask(ARB/XXX) * ask(XXX/USDT)
    // final_usdt = bid(ARB/USDT)
    // profit%    = (final_usdt / cost_usdt - 1) * 100
    
    auto arb_other_snap = getValidSnapshot(arb_pair);
    auto other_usdt_snap = getValidSnapshot(cross_pair);
    auto arb_usdt_snap = getValidSnapshot("ARB/USDT");
    
    if (!arb_other_snap.has_value() || !other_usdt_snap.has_value() || !arb_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& arb_other = arb_other_snap.value();
    const auto& other_usdt = other_usdt_snap.value();
    const auto& arb_usdt = arb_usdt_snap.value();
    
    // Calculate cost and final
    double cost_usdt = arb_other.ask_price * other_usdt.ask_price;
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
    opp.route_name = arb_pair + " -> " + cross_pair;
    opp.trade_sequence = "Buy " + arb_pair + " -> Buy " + cross_pair + " -> Sell ARB/USDT";
    opp.profit_percent = profit_percent;
    opp.arb_usdt_bid = arb_usdt.bid_price;
    opp.arb_usdt_ask = arb_usdt.ask_price;
    opp.arb_other_bid = arb_other.bid_price;
    opp.arb_other_ask = arb_other.ask_price;
    opp.other_usdt_bid = other_usdt.bid_price;
    opp.other_usdt_ask = other_usdt.ask_price;
    opp.valid = true;
    
    return opp;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkRouteDirection2(
    const std::string& arb_pair,
    const std::string& cross_pair
) const {
    // Direction 2: Buy direct, sell implied
    // cost_usdt  = ask(ARB/USDT)
    // final_usdt = bid(ARB/XXX) * bid(XXX/USDT)
    // profit%    = (final_usdt / cost_usdt - 1) * 100
    
    auto arb_usdt_snap = getValidSnapshot("ARB/USDT");
    auto arb_other_snap = getValidSnapshot(arb_pair);
    auto other_usdt_snap = getValidSnapshot(cross_pair);
    
    if (!arb_usdt_snap.has_value() || !arb_other_snap.has_value() || !other_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& arb_usdt = arb_usdt_snap.value();
    const auto& arb_other = arb_other_snap.value();
    const auto& other_usdt = other_usdt_snap.value();
    
    // Calculate cost and final
    double cost_usdt = arb_usdt.ask_price;
    double final_usdt = arb_other.bid_price * other_usdt.bid_price;
    
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
    opp.route_name = arb_pair + " -> " + cross_pair;
    opp.trade_sequence = "Buy ARB/USDT -> Sell " + arb_pair + " -> Sell " + cross_pair;
    opp.profit_percent = profit_percent;
    opp.arb_usdt_bid = arb_usdt.bid_price;
    opp.arb_usdt_ask = arb_usdt.ask_price;
    opp.arb_other_bid = arb_other.bid_price;
    opp.arb_other_ask = arb_other.ask_price;
    opp.other_usdt_bid = other_usdt.bid_price;
    opp.other_usdt_ask = other_usdt.ask_price;
    opp.valid = true;
    
    return opp;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkDirectComparison(
    const std::string& arb_stable_pair
) const {
    // Direct comparison: ARB/STABLE vs ARB/USDT
    // Direction 1: Buy ARB/STABLE, sell ARB/USDT
    // Direction 2: Buy ARB/USDT, sell ARB/STABLE
    
    auto arb_stable_snap = getValidSnapshot(arb_stable_pair);
    auto arb_usdt_snap = getValidSnapshot("ARB/USDT");
    
    if (!arb_stable_snap.has_value() || !arb_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& arb_stable = arb_stable_snap.value();
    const auto& arb_usdt = arb_usdt_snap.value();
    
    // Direction 1: Buy ARB/STABLE, sell ARB/USDT
    double cost1 = arb_stable.ask_price;
    double final1 = arb_usdt.bid_price;
    double profit1 = (final1 / cost1 - 1.0) * 100.0;
    
    // Direction 2: Buy ARB/USDT, sell ARB/STABLE
    double cost2 = arb_usdt.ask_price;
    double final2 = arb_stable.bid_price;
    double profit2 = (final2 / cost2 - 1.0) * 100.0;
    
    // Choose best direction
    bool use_direction1 = profit1 >= profit2;
    double best_profit = use_direction1 ? profit1 : profit2;
    
    if (best_profit < threshold_percent_) {
        return std::nullopt;
    }
    
    ArbitrageOpportunity opp;
    opp.direction = use_direction1 ? 1 : 2;
    opp.route_name = arb_stable_pair + " vs ARB/USDT";
    opp.trade_sequence = use_direction1 
        ? "Buy " + arb_stable_pair + " -> Sell ARB/USDT"
        : "Buy ARB/USDT -> Sell " + arb_stable_pair;
    opp.profit_percent = best_profit;
    opp.arb_usdt_bid = arb_usdt.bid_price;
    opp.arb_usdt_ask = arb_usdt.ask_price;
    opp.arb_other_bid = arb_stable.bid_price;
    opp.arb_other_ask = arb_stable.ask_price;
    opp.other_usdt_bid = 0.0;  // Not applicable for direct comparison
    opp.other_usdt_ask = 0.0;  // Not applicable for direct comparison
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

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkRoutePublic(
    const std::string& arb_pair,
    const std::string& cross_pair
) const {
    return checkRoute(arb_pair, cross_pair);
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkDirectComparisonPublic(
    const std::string& arb_stable_pair
) const {
    return checkDirectComparison(arb_stable_pair);
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkMultiLegRoute(
    const std::string& start_pair,
    const std::string& intermediate_pair,
    const std::string& final_pair
) const {
    // Multi-leg route: Start -> Intermediate -> Final
    // Example: ARB/EUR -> ARB/BTC -> BTC/USDT
    // Trade sequence: Buy ARB with EUR -> Sell ARB for BTC -> Sell BTC for USDT
    // Compare final USDT with initial EUR value (via EUR/USDT)
    
    auto start_snap = getValidSnapshot(start_pair);        // ARB/EUR
    auto intermediate_snap = getValidSnapshot(intermediate_pair); // ARB/BTC
    auto final_snap = getValidSnapshot(final_pair);        // BTC/USDT
    
    // Determine quote currency from start_pair (e.g., "ARB/EUR" -> "EUR")
    std::string quote_currency;
    size_t slash_pos = start_pair.find('/');
    if (slash_pos != std::string::npos && slash_pos + 1 < start_pair.length()) {
        quote_currency = start_pair.substr(slash_pos + 1); // "EUR"
    } else {
        return std::nullopt;
    }
    
    // Get quote/USDT pair for comparison
    std::string quote_usdt_pair = quote_currency + "/USDT";
    auto quote_usdt_snap = getValidSnapshot(quote_usdt_pair); // EUR/USDT
    
    if (!start_snap.has_value() || !intermediate_snap.has_value() || 
        !final_snap.has_value() || !quote_usdt_snap.has_value()) {
        return std::nullopt;
    }
    
    const auto& start = start_snap.value();
    const auto& intermediate = intermediate_snap.value();
    const auto& final = final_snap.value();
    const auto& quote_usdt = quote_usdt_snap.value();
    
    // Calculate: Start with 1 unit of quote currency (e.g., 1 EUR)
    // Step 1: Buy ARB with quote currency
    //   cost_quote = ask(ARB/QUOTE)  (QUOTE per ARB)
    //   arb_amount = 1.0 / cost_quote  (ARB amount for 1 QUOTE)
    
    // Step 2: Sell ARB for intermediate currency (e.g., BTC)
    //   intermediate_amount = arb_amount * bid(ARB/INTERMEDIATE)  (INTERMEDIATE amount)
    
    // Step 3: Sell intermediate for USDT
    //   final_usdt = intermediate_amount * bid(INTERMEDIATE/USDT)  (USDT amount)
    
    // Compare: final_usdt vs 1 QUOTE in USDT
    //   initial_usdt = 1.0 * ask(QUOTE/USDT)  (1 QUOTE in USDT)
    //   profit% = (final_usdt / initial_usdt - 1) * 100
    
    double cost_quote = start.ask_price; // QUOTE per ARB
    if (cost_quote <= 0.0) {
        return std::nullopt;
    }
    
    double arb_amount = 1.0 / cost_quote; // ARB amount for 1 QUOTE
    double intermediate_amount = arb_amount * intermediate.bid_price; // INTERMEDIATE amount
    double final_usdt = intermediate_amount * final.bid_price; // Final USDT
    
    double initial_usdt = 1.0 * quote_usdt.ask_price; // 1 QUOTE in USDT
    
    // Validate calculations
    if (!isValidPrice(final_usdt) || !isValidPrice(initial_usdt) || initial_usdt <= 0.0) {
        return std::nullopt;
    }
    
    // Calculate profit percentage
    double profit_percent = (final_usdt / initial_usdt - 1.0) * 100.0;
    
    // Check threshold
    if (profit_percent < threshold_percent_) {
        return std::nullopt;
    }
    
    // Create opportunity
    ArbitrageOpportunity opp;
    opp.direction = 1; // Multi-leg is always one direction
    opp.route_name = start_pair + " -> " + intermediate_pair + " -> " + final_pair;
    opp.trade_sequence = "Buy " + start_pair + " -> Sell " + intermediate_pair + " -> Sell " + final_pair;
    opp.profit_percent = profit_percent;
    
    // Store prices for display
    opp.arb_usdt_bid = 0.0; // Not applicable for multi-leg
    opp.arb_usdt_ask = 0.0;
    opp.arb_other_bid = intermediate.bid_price;
    opp.arb_other_ask = intermediate.ask_price;
    opp.other_usdt_bid = final.bid_price;
    opp.other_usdt_ask = final.ask_price;
    opp.valid = true;
    
    return opp;
}

std::optional<ArbitrageOpportunity> ArbitrageDetector::checkMultiLegRoutePublic(
    const std::string& start_pair,
    const std::string& intermediate_pair,
    const std::string& final_pair
) const {
    return checkMultiLegRoute(start_pair, intermediate_pair, final_pair);
}
