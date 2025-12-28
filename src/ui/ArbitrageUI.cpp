#include "ArbitrageUI.hpp"
#include "src/config/Symbols.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <thread>
#include <atomic>

ArbitrageUI::ArbitrageUI(MarketState& market_state, ArbitrageDetector& detector)
    : market_state_(market_state), detector_(detector) {
    ui_state_.last_update = getCurrentTime();
}

void ArbitrageUI::update() {
    std::lock_guard<std::mutex> lock(ui_state_.mutex);
    
    auto all_symbols = getAllSymbols();
    
    for (const auto& symbol : all_symbols) {
        auto snap = market_state_.get(symbol).snapshot();
        SymbolData& data = ui_state_.market_data[symbol];
        
        if (snap.has_data) {
            data.updatePrice(snap.bid_price, snap.ask_price);
            data.has_data = true;
            data.last_timestamp_ms = snap.timestamp_ms;
        } else {
            data.has_data = false;
        }
    }
    auto opportunity = detector_.checkOpportunities();
    if (opportunity.has_value() && opportunity.value().valid) {
        const auto& opp = opportunity.value();
        ui_state_.has_opportunity = true;
        ui_state_.direction = opp.direction;
        ui_state_.trade_sequence = opp.trade_sequence;
        ui_state_.route_name = opp.route_name;
        ui_state_.profit_percent = opp.profit_percent;
        ui_state_.max_tradable_amount = opp.max_tradable_amount;
        ui_state_.max_tradable_currency = opp.max_tradable_currency;
        ui_state_.opportunities_found++;
        
        if (opp.profit_percent > ui_state_.max_profit_found) {
            ui_state_.max_profit_found = opp.profit_percent;
        }
        if (ui_state_.opportunities_found > 0) {
            ui_state_.avg_profit_found = 
                (ui_state_.avg_profit_found * (ui_state_.opportunities_found - 1) + opp.profit_percent) 
                / ui_state_.opportunities_found;
        }
    } else {
        ui_state_.has_opportunity = false;
    }
    
    ui_state_.route_statuses.clear();
    
    // 2-leg routes
    std::vector<std::pair<std::string, std::string>> routes = {
        {"ARB/BTC", "BTC/USDT"},
        {"ARB/ETH", "ETH/USDT"},
        {"ARB/EUR", "EUR/USDT"},
        {"ARB/TRY", "TRY/USDT"}
    };
    
    for (const auto& [arb_pair, cross_pair] : routes) {
        UIState::RouteStatus status;
        status.route_name = arb_pair + " -> " + cross_pair;
        
        auto arb_snap = market_state_.get(arb_pair).snapshot();
        auto cross_snap = market_state_.get(cross_pair).snapshot();
        auto usdt_snap = market_state_.get("ARB/USDT").snapshot();
        
        status.has_data = arb_snap.has_data && cross_snap.has_data && usdt_snap.has_data;
        
        if (status.has_data) {
            // Check both directions and get best profit
            auto route_opp = detector_.checkRoutePublic(arb_pair, cross_pair);
            if (route_opp.has_value() && route_opp.value().valid) {
                status.has_opportunity = true;
                status.profit_percent = route_opp.value().profit_percent;
            } else {
                // Calculate current profit even if below threshold
                // Direction 1: Buy implied, sell direct
                double cost1 = arb_snap.ask_price * cross_snap.ask_price;
                double final1 = usdt_snap.bid_price;
                double profit1 = cost1 > 0 ? (final1 / cost1 - 1.0) * 100.0 : 0.0;
                
                // Direction 2: Buy direct, sell implied
                double cost2 = usdt_snap.ask_price;
                double final2 = arb_snap.bid_price * cross_snap.bid_price;
                double profit2 = cost2 > 0 ? (final2 / cost2 - 1.0) * 100.0 : 0.0;
                
                status.profit_percent = std::max(profit1, profit2);
            }
        }
        
        ui_state_.route_statuses.push_back(status);
    }
    
    // Direct comparison routes
    std::vector<std::string> direct_routes = {"ARB/FDUSD", "ARB/USDC", "ARB/TUSD"};
    for (const auto& stable_pair : direct_routes) {
        UIState::RouteStatus status;
        status.route_name = stable_pair + " vs ARB/USDT";
        
        auto stable_snap = market_state_.get(stable_pair).snapshot();
        auto usdt_snap = market_state_.get("ARB/USDT").snapshot();
        
        status.has_data = stable_snap.has_data && usdt_snap.has_data;
        
        if (status.has_data) {
            auto direct_opp = detector_.checkDirectComparisonPublic(stable_pair);
            if (direct_opp.has_value() && direct_opp.value().valid) {
                status.has_opportunity = true;
                status.profit_percent = direct_opp.value().profit_percent;
            } else {
                // Calculate current profit even if below threshold
                double cost1 = stable_snap.ask_price;
                double final1 = usdt_snap.bid_price;
                double profit1 = cost1 > 0 ? (final1 / cost1 - 1.0) * 100.0 : 0.0;
                
                double cost2 = usdt_snap.ask_price;
                double final2 = stable_snap.bid_price;
                double profit2 = cost2 > 0 ? (final2 / cost2 - 1.0) * 100.0 : 0.0;
                
                status.profit_percent = std::max(profit1, profit2);
            }
        }
        
        ui_state_.route_statuses.push_back(status);
    }
    
    // Multi-leg routes (3+ legs)
    std::vector<std::tuple<std::string, std::string, std::string>> multi_leg_routes = {
        {"ARB/EUR", "ARB/BTC", "BTC/USDT"},
        {"ARB/EUR", "ARB/ETH", "ETH/USDT"},
        {"ARB/TRY", "ARB/BTC", "BTC/USDT"},
        {"ARB/TRY", "ARB/ETH", "ETH/USDT"}
    };
    
    for (const auto& [start, intermediate, final] : multi_leg_routes) {
        UIState::RouteStatus status;
        status.route_name = start + " -> " + intermediate + " -> " + final;
        
        // Check if we have data for this route
        auto start_snap = market_state_.get(start).snapshot();
        auto intermediate_snap = market_state_.get(intermediate).snapshot();
        auto final_snap = market_state_.get(final).snapshot();
        
        // Determine quote currency
        std::string quote_currency;
        size_t slash_pos = start.find('/');
        if (slash_pos != std::string::npos && slash_pos + 1 < start.length()) {
            quote_currency = start.substr(slash_pos + 1);
        }
        std::string quote_usdt_pair = quote_currency + "/USDT";
        auto quote_usdt_snap = market_state_.get(quote_usdt_pair).snapshot();
        
        status.has_data = start_snap.has_data && intermediate_snap.has_data && 
                         final_snap.has_data && quote_usdt_snap.has_data;
        
        if (status.has_data) {
            auto multi_leg_opp = detector_.checkMultiLegRoutePublic(start, intermediate, final);
            if (multi_leg_opp.has_value() && multi_leg_opp.value().valid) {
                status.has_opportunity = true;
                status.profit_percent = multi_leg_opp.value().profit_percent;
            } else {
                // Calculate current profit even if below threshold
                double cost_quote = start_snap.ask_price;
                if (cost_quote > 0.0) {
                    double arb_amount = 1.0 / cost_quote;
                    double intermediate_amount = arb_amount * intermediate_snap.bid_price;
                    double final_usdt = intermediate_amount * final_snap.bid_price;
                    double initial_usdt = 1.0 * quote_usdt_snap.ask_price;
                    
                    if (initial_usdt > 0.0) {
                        status.profit_percent = (final_usdt / initial_usdt - 1.0) * 100.0;
                    }
                }
            }
        }
        
        ui_state_.route_statuses.push_back(status);
    }
    
    // Update symbol statistics
    int active_count = 0;
    int stale_count = 0;
    int total_count = 0;
    
    for (const auto& [symbol, data] : ui_state_.market_data) {
        total_count++;
        if (data.has_data) {
            if (data.isStale(3000)) {
                stale_count++;
            } else {
                active_count++;
            }
        }
    }
    
    ui_state_.active_symbols_count = active_count;
    ui_state_.stale_symbols_count = stale_count;
    ui_state_.total_symbols_count = total_count;
    
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
    
    component |= ftxui::CatchEvent([this, &running, &screen](ftxui::Event event) {
        if (event == ftxui::Event::Character('q') || event == ftxui::Event::Escape) {
            running = false;
            screen.Exit();
            return true;
        }
        
        if (event.is_mouse()) {
            std::lock_guard<std::mutex> lock(ui_state_.mutex);
            
            if (event.mouse().button == ftxui::Mouse::WheelDown) {
                ui_state_.scroll_offset += 1;
                screen.PostEvent(ftxui::Event::Custom);
                return true;
            } else if (event.mouse().button == ftxui::Mouse::WheelUp) {
                ui_state_.scroll_offset = std::max(0, ui_state_.scroll_offset - 1);
                screen.PostEvent(ftxui::Event::Custom);
                return true;
            }
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
            text("Arbitrage Detection Monitor") | bold | center | color(Color::Cyan),
        }) | border | color(Color::Cyan) | size(ftxui::WIDTH, ftxui::EQUAL, 900) | size(ftxui::HEIGHT, ftxui::GREATER_THAN, 8);
        
        // Market Prices Section - Show all symbols
        Elements price_boxes;
        price_boxes.push_back(text("Market Prices") | bold | color(Color::Yellow));
        price_boxes.push_back(separator());
        
        auto all_symbols = getAllSymbols();
        Elements arb_pairs_row;
        Elements cross_pairs_row;
        
        for (const auto& symbol : all_symbols) {
            auto it = state.market_data.find(symbol);
            if (it != state.market_data.end()) {
                const auto& data = it->second;
                
                bool is_stale = data.isStale(3000);
                bool show_as_active = data.has_data && !is_stale;
                
                std::string status_text = show_as_active ? "●" : "○";
                
                int precision = 8;
                if (symbol.find("BTC/") == 0 || symbol.find("ETH/") == 0) {
                    precision = 2;
                } else if (symbol.find("EUR/") == 0 || symbol.find("TRY/") == 0) {
                    precision = 4;
                }
                
                Element price_box = vbox({
                    text(symbol) | bold,
                    hbox({
                        text(status_text + " "),
                        vbox({
                            text("Bid: " + formatPrice(data.bid_price, precision)),
                            text("Ask: " + formatPrice(data.ask_price, precision)),
                        }),
                    }),
                }) | border;
                
                // Color based on price change or staleness
                if (!data.has_data) {
                    // No data: gray (dim)
                    price_box = price_box | dim;
                } else if (is_stale) {
                    // Stale (3 seconds no update): white (default, no color)
                    // No color applied, just default white
                } else {
                    // Active data: color based on price change
                    switch (data.price_change) {
                        case PriceChange::Up:
                            price_box = price_box | color(Color::Green);
                            break;
                        case PriceChange::Down:
                            price_box = price_box | color(Color::Red);
                            break;
                        case PriceChange::Stable:
                        case PriceChange::Unknown:
                        default:
                            // Stable or unknown: white (default, no color)
                            break;
                    }
                }
                
                // Separate ARB pairs from cross pairs
                if (symbol.find("ARB/") == 0) {
                    arb_pairs_row.push_back(price_box);
                } else {
                    cross_pairs_row.push_back(price_box);
                }
            }
        }
        
        // Add ARB pairs row
        if (!arb_pairs_row.empty()) {
            price_boxes.push_back(text("ARB Trading Pairs") | dim);
            price_boxes.push_back(hbox(arb_pairs_row));
            price_boxes.push_back(separator());
        }
        
        // Add cross pairs row
        if (!cross_pairs_row.empty()) {
            price_boxes.push_back(text("Cross Pairs (for Implied USDT)") | dim);
            price_boxes.push_back(hbox(cross_pairs_row));
        }
        
        auto market_section = vbox(price_boxes) | border | size(ftxui::WIDTH, ftxui::EQUAL, 900) | size(ftxui::HEIGHT, ftxui::GREATER_THAN, 35);
        
        // Arbitrage Opportunity Section
        Element opportunity_section;
        if (state.has_opportunity) {
            auto profit_color = state.profit_percent > 0.5 ? Color::Green : Color::Yellow;
            std::string max_tradable_text = "Max Tradable: " + formatPrice(state.max_tradable_amount, 2) + " " + state.max_tradable_currency;
            opportunity_section = vbox({
                text("ARBITRAGE OPPORTUNITY DETECTED!") | bold | color(Color::Red),
                separator(),
                text("Route: " + state.route_name),
                text("Direction: " + std::to_string(state.direction)),
                text("Trade Sequence: " + state.trade_sequence),
                text("Profit: " + formatPrice(state.profit_percent, 4) + "%") | color(profit_color) | bold,
                text(max_tradable_text) | color(Color::Cyan),
            }) | border | color(Color::Red) | size(ftxui::WIDTH, ftxui::EQUAL, 900) | size(ftxui::HEIGHT, ftxui::GREATER_THAN, 15);
        } else {
            opportunity_section = vbox({
                text("No arbitrage opportunity") | dim,
                text("(Threshold: 0.10%)") | dim,
            }) | border | size(ftxui::WIDTH, ftxui::EQUAL, 900) | size(ftxui::HEIGHT, ftxui::GREATER_THAN, 15);
        }
        
        // Route Status Section
        Elements route_elements;
        route_elements.push_back(text("Route Status") | bold | color(Color::Cyan));
        route_elements.push_back(separator());
        
        for (const auto& route : state.route_statuses) {
            std::string route_text = route.route_name;
            std::string profit_text;
            
            if (!route.has_data) {
                profit_text = "N/A (no data)";
                route_text += ": " + profit_text;
                route_elements.push_back(text(route_text) | dim);
            } else {
                profit_text = formatPrice(route.profit_percent, 4) + "%";
                
                if (route.has_opportunity) {
                    // Above threshold - show in green/yellow based on profit
                    auto profit_color = route.profit_percent > 0.5 ? Color::Green : Color::Yellow;
                    route_text += ": " + profit_text + " ✓";
                    route_elements.push_back(text(route_text) | color(profit_color) | bold);
                } else {
                    // Below threshold - show in white/gray
                    route_text += ": " + profit_text;
                    route_elements.push_back(text(route_text));
                }
            }
        }
        
        auto route_section = vbox(route_elements) | border | size(ftxui::WIDTH, ftxui::EQUAL, 900) | size(ftxui::HEIGHT, ftxui::GREATER_THAN, 25);
        
        // Statistics Section
        Elements stats_elements;
        stats_elements.push_back(text("Statistics") | bold | color(Color::Cyan));
        stats_elements.push_back(separator());
        
        // Performance metrics
        stats_elements.push_back(text("Performance:") | dim);
        stats_elements.push_back(text("  Checks performed: " + std::to_string(state.check_count)));
        double success_rate = state.check_count > 0 
            ? (static_cast<double>(state.opportunities_found) / state.check_count * 100.0)
            : 0.0;
        stats_elements.push_back(text("  Success rate: " + formatPrice(success_rate, 2) + "%"));
        stats_elements.push_back(separator());
        
        // Opportunity metrics
        stats_elements.push_back(text("Opportunities:") | dim);
        stats_elements.push_back(text("  Total found: " + std::to_string(state.opportunities_found)));
        if (state.max_profit_found > 0.0) {
            stats_elements.push_back(text("  Max profit: " + formatPrice(state.max_profit_found, 4) + "%") | color(Color::Green));
        }
        if (state.avg_profit_found > 0.0) {
            stats_elements.push_back(text("  Avg profit: " + formatPrice(state.avg_profit_found, 4) + "%"));
        }
        stats_elements.push_back(separator());
        
        // Data quality metrics
        stats_elements.push_back(text("Data Quality:") | dim);
        stats_elements.push_back(text("  Active symbols: " + std::to_string(state.active_symbols_count) + "/" + std::to_string(state.total_symbols_count)) | color(Color::Green));
        if (state.stale_symbols_count > 0) {
            stats_elements.push_back(text("  Stale symbols: " + std::to_string(state.stale_symbols_count)) | color(Color::Yellow));
        }
        stats_elements.push_back(separator());
        
        // Timestamp
        stats_elements.push_back(text("Last update: " + state.last_update));
        
        auto stats_section = vbox(stats_elements) | border;
        
        // Footer
        auto footer = text("Press 'q' to quit | Mouse wheel to scroll") | center | dim;
        
        // Main content
        Elements content_elements;
        content_elements.push_back(header);
        content_elements.push_back(separator());
        content_elements.push_back(market_section);
        content_elements.push_back(separator());
        content_elements.push_back(opportunity_section);
        content_elements.push_back(separator());
        content_elements.push_back(route_section);
        content_elements.push_back(separator());
        content_elements.push_back(stats_section);
        content_elements.push_back(separator());
        content_elements.push_back(footer);
        
        Elements scrolled_elements;
        if (state.scroll_offset > 0 && state.scroll_offset < static_cast<int>(content_elements.size())) {
            scrolled_elements.insert(scrolled_elements.end(), 
                content_elements.begin() + state.scroll_offset, 
                content_elements.end());
        } else if (state.scroll_offset >= static_cast<int>(content_elements.size())) {
            if (!content_elements.empty()) {
                scrolled_elements.push_back(content_elements.back());
            }
        } else {
            scrolled_elements = content_elements;
        }
        
        auto main_content = vbox(scrolled_elements);
        return main_content;
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

std::vector<std::string> ArbitrageUI::getAllSymbols() const {
    return Symbols::getAllSymbols();
}
