#include "ArbitrageUI.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <atomic>

ArbitrageUI::ArbitrageUI(MarketState& market_state, ArbitrageDetector& detector)
    : market_state_(market_state), detector_(detector) {}

void ArbitrageUI::update() {
    std::lock_guard<std::mutex> lock(ui_state_.mutex);
    
    // Update market data
    auto arb_usdt = market_state_.get("ARB/USDT").snapshot();
    auto arb_btc = market_state_.get("ARB/BTC").snapshot();
    auto btc_usdt = market_state_.get("BTC/USDT").snapshot();
    
    if (arb_usdt.has_data) {
        ui_state_.arb_usdt_bid = arb_usdt.bid_price;
        ui_state_.arb_usdt_ask = arb_usdt.ask_price;
    }
    
    if (arb_btc.has_data) {
        ui_state_.arb_btc_bid = arb_btc.bid_price;
        ui_state_.arb_btc_ask = arb_btc.ask_price;
    }
    
    if (btc_usdt.has_data) {
        ui_state_.btc_usdt_bid = btc_usdt.bid_price;
        ui_state_.btc_usdt_ask = btc_usdt.ask_price;
    }
    
    // Check for arbitrage opportunity
    auto opportunity = detector_.checkOpportunities();
    if (opportunity.has_value() && opportunity.value().valid) {
        const auto& opp = opportunity.value();
        ui_state_.has_opportunity = true;
        ui_state_.direction = opp.direction;
        ui_state_.trade_sequence = opp.trade_sequence;
        ui_state_.profit_percent = opp.profit_percent;
        ui_state_.opportunities_found++;
    } else {
        ui_state_.has_opportunity = false;
    }
    
    // Update statistics
    ui_state_.check_count = detector_.getCheckCount();
    ui_state_.last_update = getCurrentTime();
}

void ArbitrageUI::run() {
    auto screen = ftxui::ScreenInteractive::Fullscreen();
    auto component = buildComponent();
    
    std::atomic<bool> running{true};
    
    // Update thread
    std::thread update_thread([this, &screen, &running]() {
        while (running.load()) {
            update();
            screen.PostEvent(ftxui::Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
    
    // Exit handler
    component |= ftxui::CatchEvent([&running, &screen](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            running = false;
            screen.Exit();
            return true;
        }
        return false;
    });
    
    screen.Loop(component);
    running = false;
    if (update_thread.joinable()) {
        update_thread.join();
    }
}

ftxui::Component ArbitrageUI::buildComponent() {
    using namespace ftxui;
    
    auto renderer = Renderer([this]() {
        std::lock_guard<std::mutex> lock(ui_state_.mutex);
        const auto& state = ui_state_;
        
        // Header
        auto header = hbox({
            text("Arbitrage Detection Monitor") | bold | center,
        }) | border | color(Color::Cyan);
        
        // Market Prices Section
        auto market_section = vbox({
            text("Market Prices") | bold | color(Color::Yellow),
            separator(),
            hbox({
                vbox({
                    text("ARB/USDT"),
                    text("Bid: " + formatPrice(state.arb_usdt_bid)),
                    text("Ask: " + formatPrice(state.arb_usdt_ask)),
                }) | border,
                vbox({
                    text("ARB/BTC"),
                    text("Bid: " + formatPrice(state.arb_btc_bid)),
                    text("Ask: " + formatPrice(state.arb_btc_ask)),
                }) | border,
                vbox({
                    text("BTC/USDT"),
                    text("Bid: " + formatPrice(state.btc_usdt_bid)),
                    text("Ask: " + formatPrice(state.btc_usdt_ask)),
                }) | border,
            }),
        }) | border;
        
        // Arbitrage Opportunity Section
        Element opportunity_section;
        if (state.has_opportunity) {
            auto profit_color = state.profit_percent > 0.5 ? Color::Green : Color::Yellow;
            opportunity_section = vbox({
                text("ARBITRAGE OPPORTUNITY DETECTED!") | bold | color(Color::Red),
                separator(),
                text("Direction: " + std::to_string(state.direction)),
                text("Trade Sequence: " + state.trade_sequence),
                text("Profit: " + formatPrice(state.profit_percent, 4) + "%") | color(profit_color) | bold,
            }) | border | color(Color::Red);
        } else {
            opportunity_section = vbox({
                text("No arbitrage opportunity") | dim,
                text("(Threshold: 0.10%)") | dim,
            }) | border;
        }
        
        // Statistics Section
        auto stats_section = vbox({
            text("Statistics") | bold | color(Color::Cyan),
            separator(),
            text("Checks performed: " + std::to_string(state.check_count)),
            text("Opportunities found: " + std::to_string(state.opportunities_found)),
            text("Last update: " + state.last_update),
        }) | border;
        
        // Footer
        auto footer = text("Press 'q' to quit") | center | dim;
        
        return vbox({
            header,
            separator(),
            market_section,
            separator(),
            opportunity_section,
            separator(),
            stats_section,
            separator(),
            footer,
        });
    });
    
    return renderer;
}

std::string ArbitrageUI::formatPrice(double price, int precision) const {
    if (price <= 0.0) {
        return "N/A";
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << price;
    return oss.str();
}

std::string ArbitrageUI::getCurrentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}
