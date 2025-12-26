#include "Router.hpp"

const std::vector<std::string> Router::ARB_PAIRS = {
    "ARB/USDT", "ARB/FDUSD", "ARB/USDC", "ARB/TUSD",
    "ARB/BTC", "ARB/ETH", "ARB/TRY", "ARB/EUR"
};

const std::vector<std::string> Router::INTERMEDIATE_PAIRS = {
    "BTC/USDT", "ETH/USDT", "EUR/USDT", "TRY/USDT",
    "EUR/BTC", "TRY/BTC", "FDUSD/USDT", "USDC/USDT", "TUSD/USDT"
};

Router::Router() {
}

std::vector<TradePath> Router::generateDirectPaths(
    const std::string& arb_pair) const {
    
    std::vector<TradePath> paths;
    
    if (arb_pair == "ARB/USDT") {
        return paths;
    }
    
    TradePath path;
    path.steps = {arb_pair, "ARB/USDT"};
    path.is_buy = {true, false};
        path.description = "Buy " + arb_pair + " -> Sell ARB/USDT";
    paths.push_back(path);
    
    return paths;
}

std::vector<TradePath> Router::generateTwoLegPaths(
    const std::string& arb_pair) const {
    
    std::vector<TradePath> paths;
    
    if (arb_pair == "ARB/USDT") {
        return paths;
    }
    
    size_t slash_pos = arb_pair.find('/');
    if (slash_pos == std::string::npos) {
        return paths;
    }
    
    std::string quote = arb_pair.substr(slash_pos + 1);
    
    if (quote == "USDT") {
        return paths;
    }
    
    std::string intermediate_pair = quote + "/USDT";
    
    TradePath path;
    path.steps = {arb_pair, intermediate_pair};
    path.is_buy = {true, false};
        path.description = "Buy " + arb_pair + " -> Sell " + intermediate_pair;
    paths.push_back(path);
    
    return paths;
}

std::vector<TradePath> Router::generateTriangularPaths(
    const std::string& arb_pair) const {
    
    std::vector<TradePath> paths;
    
    if (arb_pair == "ARB/USDT") {
        return paths;
    }
    
    size_t slash_pos = arb_pair.find('/');
    if (slash_pos == std::string::npos) {
        return paths;
    }
    
    std::string quote = arb_pair.substr(slash_pos + 1);
    
    // PDF'ye göre: ARB/BTC → BTC/USDT (2-leg, triangular değil)
    if (quote == "BTC" || quote == "ETH") {
        std::string intermediate = quote + "/USDT";
        TradePath path;
        path.steps = {arb_pair, intermediate};
        path.is_buy = {true, false};
        path.description = "Buy " + arb_pair + " -> Sell " + intermediate;
        paths.push_back(path);
        return paths;
    }
    
    // PDF'ye göre triangular arbitrage:
    // "Buy ARB with EUR → Sell ARB for BTC → Sell BTC for USDT"
    // Yani: ARB/EUR → ARB/BTC → BTC/USDT
    std::vector<std::string> bridges = {"BTC", "ETH"};
    
    for (const auto& bridge : bridges) {
        // Triangular path: ARB/QUOTE → ARB/BRIDGE → BRIDGE/USDT
        std::string arb_bridge_pair = "ARB/" + bridge;
        std::string bridge_usdt_pair = bridge + "/USDT";
        
        TradePath path;
        path.steps = {arb_pair, arb_bridge_pair, bridge_usdt_pair};
        path.is_buy = {true, false, false};
        path.description = "Buy " + arb_pair + " -> Sell " + arb_bridge_pair 
                          + " -> Sell " + bridge_usdt_pair;
        paths.push_back(path);
    }
    
    return paths;
}

std::vector<TradePath> Router::generateMultiLegPaths(
    const std::string& arb_pair) const {
    
    std::vector<TradePath> paths;
    
    auto triangular = generateTriangularPaths(arb_pair);
    paths.insert(paths.end(), triangular.begin(), triangular.end());
    
    return paths;
}

std::vector<TradePath> Router::generateAllPaths(
    const std::string& arb_pair) const {
    
    std::vector<TradePath> all_paths;
    
    auto direct = generateDirectPaths(arb_pair);
    all_paths.insert(all_paths.end(), direct.begin(), direct.end());
    
    auto two_leg = generateTwoLegPaths(arb_pair);
    all_paths.insert(all_paths.end(), two_leg.begin(), two_leg.end());
    
    auto multi_leg = generateMultiLegPaths(arb_pair);
    all_paths.insert(all_paths.end(), multi_leg.begin(), multi_leg.end());
    
    return all_paths;
}

