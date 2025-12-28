#pragma once

#include "src/core/MarketState.hpp"
#include "src/core/ArbitrageDetector.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

enum class PriceChange {
    Up,      // Price increased (green)
    Down,    // Price decreased (red)
    Stable,  // No change or stale (white)
    Unknown  // No previous data
};

struct SymbolData {
    double bid_price = 0.0;
    double ask_price = 0.0;
    double previous_bid_price = 0.0;
    double previous_ask_price = 0.0;
    bool has_data = false;
    int64_t last_timestamp_ms = 0;  // Last timestamp from OrderBook
    PriceChange price_change = PriceChange::Unknown;
    
    SymbolData() 
        : bid_price(0.0), ask_price(0.0), 
          previous_bid_price(0.0), previous_ask_price(0.0),
          has_data(false), last_timestamp_ms(0), 
          price_change(PriceChange::Unknown) {}
    
    // Check if data is stale (older than threshold_ms)
    bool isStale(int64_t threshold_ms = 3000) const {
        if (!has_data || last_timestamp_ms == 0) {
            return true;
        }
        
        // Get current time in milliseconds (epoch)
        auto now = std::chrono::system_clock::now();
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        // Check if timestamp is in the future (shouldn't happen, but safety check)
        if (last_timestamp_ms > now_ms) {
            return false; // Future timestamp, consider it fresh
        }
        
        // Check if data is older than threshold
        int64_t elapsed = now_ms - last_timestamp_ms;
        return elapsed > threshold_ms;
    }
    
    // Update price and determine change direction
    void updatePrice(double new_bid, double new_ask) {
        // Store previous prices
        previous_bid_price = bid_price;
        previous_ask_price = ask_price;
        
        // Update current prices
        bid_price = new_bid;
        ask_price = new_ask;
        
        // Determine price change (use mid price for comparison)
        if (previous_bid_price > 0.0 && previous_ask_price > 0.0) {
            double previous_mid = (previous_bid_price + previous_ask_price) / 2.0;
            double current_mid = (new_bid + new_ask) / 2.0;
            
            const double EPSILON = 1e-10; // Small threshold for "no change"
            if (current_mid > previous_mid + EPSILON) {
                price_change = PriceChange::Up;
            } else if (current_mid < previous_mid - EPSILON) {
                price_change = PriceChange::Down;
            } else {
                price_change = PriceChange::Stable;
            }
        } else {
            price_change = PriceChange::Unknown;
        }
    }
};

struct UIState {
    // Market data - all symbols
    std::unordered_map<std::string, SymbolData> market_data;
    
    // Arbitrage opportunity
    bool has_opportunity = false;
    int direction = 0;
    std::string trade_sequence;
    std::string route_name;
    double profit_percent = 0.0;
    double max_tradable_amount = 0.0;
    std::string max_tradable_currency;
    
    // Route status (for each route, show current profit if any)
    struct RouteStatus {
        std::string route_name;
        double profit_percent;
        bool has_opportunity;
        bool has_data;
        
        RouteStatus() : profit_percent(0.0), has_opportunity(false), has_data(false) {}
    };
    std::vector<RouteStatus> route_statuses;
    
    // Statistics
    int check_count = 0;
    int opportunities_found = 0;
    double max_profit_found = 0.0;
    double avg_profit_found = 0.0;
    int active_symbols_count = 0;
    int stale_symbols_count = 0;
    int total_symbols_count = 0;
    std::string uptime;  // How long the system has been running
    
    // Timestamp
    std::string last_update;
    
    // Scroll offset for manual scrolling
    int scroll_offset = 0;
    
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
    
    // Get all symbols to display
    std::vector<std::string> getAllSymbols() const;
};
