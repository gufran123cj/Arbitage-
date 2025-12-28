#pragma once

#include "src/core/MarketState.hpp"
#include "src/core/ArbitrageDetector.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>

struct UIState {
    // Market data
    double arb_usdt_bid = 0.0;
    double arb_usdt_ask = 0.0;
    double arb_btc_bid = 0.0;
    double arb_btc_ask = 0.0;
    double btc_usdt_bid = 0.0;
    double btc_usdt_ask = 0.0;
    
    // Arbitrage opportunity
    bool has_opportunity = false;
    int direction = 0;
    std::string trade_sequence;
    double profit_percent = 0.0;
    
    // Statistics
    int check_count = 0;
    int opportunities_found = 0;
    
    // Timestamp
    std::string last_update;
    
    std::mutex mutex;
};

class ArbitrageUI {
public:
    ArbitrageUI(MarketState& market_state, ArbitrageDetector& detector);
    
    // Update UI state from market data
    void update();
    
    // Run the UI (blocking call)
    void run();

private:
    MarketState& market_state_;
    ArbitrageDetector& detector_;
    UIState ui_state_;
    
    // Build the UI component
    ftxui::Component buildComponent();
    
    // Format price for display
    std::string formatPrice(double price, int precision = 8) const;
    
    // Get current timestamp string
    std::string getCurrentTime() const;
};
