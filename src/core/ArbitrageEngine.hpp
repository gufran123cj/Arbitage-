#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <optional>
#include "MarketUpdate.hpp"
#include "../util/ConcurrentQueue.hpp"
#include "OrderBook.hpp"
#include "PriceCalculator.hpp"
#include "Router.hpp"
#include "../util/Logger.hpp"

struct ArbitrageOpportunity {
    std::string trade_sequence;
    double profit_percentage;
    std::string price_levels;
    double max_tradable_amount;
    std::string arb_pair;
};

class ArbitrageEngine {
public:
    ArbitrageEngine();
    ~ArbitrageEngine();

    ArbitrageEngine(const ArbitrageEngine&) = delete;
    ArbitrageEngine& operator=(const ArbitrageEngine&) = delete;

    void start();
    void stop();
    void processUpdate(const MarketUpdate& update);

    void setUpdateQueue(
        std::shared_ptr<ConcurrentQueue<MarketUpdate>> queue);

private:
    static constexpr double PROFIT_THRESHOLD = 0.1;
    static constexpr int ORDER_BOOK_DEPTH = 5;
    static const std::vector<std::string> ARB_PAIRS;

    std::atomic<bool> running_;
    std::thread engine_thread_;
    
    std::shared_ptr<ConcurrentQueue<MarketUpdate>> update_queue_;
    
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> order_books_;
    std::unique_ptr<PriceCalculator> price_calculator_;
    std::unique_ptr<Router> router_;

    void engineLoop();
    void processUpdates();
    void detectArbitrage();
    
    std::optional<ArbitrageOpportunity> checkPath(
        const TradePath& path,
        const std::string& arb_pair) const;
    
    double calculateMaxTradableAmount(
        const TradePath& path) const;
    
    std::string formatPriceLevels(
        const TradePath& path) const;
    
    // Adım 5: Tek rota testi
    void testSingleRoute() const;
};

