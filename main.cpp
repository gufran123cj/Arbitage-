#include "src/net/WebSocketClient.hpp"
#include "src/core/MarketState.hpp"
#include "src/core/ArbitrageDetector.hpp"
#include "src/ui/ArbitrageUI.hpp"
#include "src/util/ArbitrageLogger.hpp"
#include "src/config/Symbols.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <iostream>

int main() {
    MarketState market_state;
    
    // Get all symbols to monitor
    auto all_symbols = Symbols::getAllSymbols();
    
    std::cout << "Starting WebSocket clients for " << all_symbols.size() << " symbols..." << std::endl;
    
    // Start WebSocket clients for all symbols
    std::vector<std::unique_ptr<WebSocketClient>> clients;
    
    for (const auto& symbol : all_symbols) {
        std::string stream = Symbols::toBinanceStream(symbol);
        std::cout << "  Connecting to: " << symbol << " (" << stream << ")" << std::endl;
        
        clients.push_back(std::make_unique<WebSocketClient>(stream, market_state));
        clients.back()->start();
        
        // Small delay between connections to avoid rate limiting
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "All WebSocket clients started. Waiting for initial data..." << std::endl;
    
    // Wait a bit for initial data
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    
    // Create arbitrage detector with 0.10% threshold
    ArbitrageDetector detector(market_state, 0.10);
    
    // Create logger for saving opportunities to JSON
    ArbitrageLogger logger;
    
    // Create and run UI
    ArbitrageUI ui(market_state, detector);
    
    // Start arbitrage checking thread
    std::thread check_thread([&detector, &logger]() {
        while (true) {
            detector.incrementCheckCount();
            auto opportunity = detector.checkOpportunities();
            if (opportunity.has_value() && opportunity.value().valid) {
                // Log opportunity to JSON file
                logger.logOpportunity(opportunity.value());
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    // Run UI (blocking)
    ui.run();
    
    // Cleanup
    check_thread.detach();
    std::cout << "Stopping WebSocket clients..." << std::endl;
    for (auto& client : clients) {
        client->stop();
    }
    
    return 0;
}
