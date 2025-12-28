#pragma once

#include <vector>
#include <string>

namespace Symbols {
    // All ARB trading pairs required by case study
    const std::vector<std::string> ARB_PAIRS = {
        "ARB/USDT",   // Direct USDT pair
        "ARB/BTC",    // Bitcoin pair
        "ARB/ETH",    // Ethereum pair
        "ARB/FDUSD",  // First Digital USD
        "ARB/USDC",   // USD Coin
        "ARB/TUSD",   // TrueUSD
        "ARB/TRY",    // Turkish Lira
        "ARB/EUR"     // Euro
    };
    
    // Cross pairs for implied USDT calculation
    const std::vector<std::string> CROSS_PAIRS = {
        "BTC/USDT",   // For ARB/BTC -> ARB/USDT
        "ETH/USDT",   // For ARB/ETH -> ARB/USDT
        "EUR/USDT",   // For ARB/EUR -> ARB/USDT (if available)
        "TRY/USDT"    // For ARB/TRY -> ARB/USDT (if available)
    };
    
    // Convert symbol to Binance WebSocket stream format
    // ARB/USDT -> arbusdt@bookTicker
    std::string toBinanceStream(const std::string& symbol);
    
    // Get all symbols that need to be monitored
    std::vector<std::string> getAllSymbols();
}
