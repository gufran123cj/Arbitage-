#include "ArbitrageEngine.hpp"
#include "Router.hpp"
#include "../util/Logger.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <optional>
#include <limits>
#include <thread>
#include <chrono>

const std::vector<std::string> ArbitrageEngine::ARB_PAIRS = {
    "ARB/USDT", "ARB/FDUSD", "ARB/USDC", "ARB/TUSD",
    "ARB/BTC", "ARB/ETH", "ARB/TRY", "ARB/EUR"
};

ArbitrageEngine::ArbitrageEngine()
    : running_(false),
      price_calculator_(std::make_unique<PriceCalculator>()),
      router_(std::make_unique<Router>()) {
    
    for (const auto& pair : ARB_PAIRS) {
        auto order_book = std::make_shared<OrderBook>(pair);
        order_books_[pair] = order_book;
        price_calculator_->registerOrderBook(pair, order_book);
    }
    
    std::vector<std::string> intermediate_pairs = {
        "BTC/USDT", "ETH/USDT", "EUR/USDT", "TRY/USDT",
        "EUR/BTC", "TRY/BTC", "FDUSD/USDT", "USDC/USDT", "TUSD/USDT"
    };
    
    for (const auto& pair : intermediate_pairs) {
        auto order_book = std::make_shared<OrderBook>(pair);
        order_books_[pair] = order_book;
        price_calculator_->registerOrderBook(pair, order_book);
    }
}

ArbitrageEngine::~ArbitrageEngine() {
    stop();
}

void ArbitrageEngine::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    engine_thread_ = std::thread(&ArbitrageEngine::engineLoop, this);
}

void ArbitrageEngine::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    if (engine_thread_.joinable()) {
        engine_thread_.join();
    }
}

void ArbitrageEngine::setUpdateQueue(
    std::shared_ptr<ConcurrentQueue<MarketUpdate>> queue) {
    
    update_queue_ = queue;
}

void ArbitrageEngine::processUpdate(const MarketUpdate& update) {
    auto it = order_books_.find(update.symbol);
    if (it == order_books_.end()) {
        return;
    }
    
    if (update.is_snapshot) {
        it->second->applySnapshot(update.bids, update.asks);
    } else {
        it->second->applyUpdate(update.bids, update.asks);
    }
}

void ArbitrageEngine::engineLoop() {
    LOG_INFO("ArbitrageEngine loop started");
    try {
        while (running_.load()) {
            try {
                processUpdates();
                detectArbitrage();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error in engine loop: ") + e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (...) {
                LOG_ERROR("Unknown error in engine loop");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (...) {
        LOG_ERROR("Fatal error in engine loop");
    }
    LOG_INFO("ArbitrageEngine loop stopped");
}

void ArbitrageEngine::processUpdates() {
    if (!update_queue_) {
        return;
    }
    
    try {
        MarketUpdate update;
        int update_count = 0;
        while (update_queue_->tryPop(update)) {
            try {
                processUpdate(update);
                update_count++;
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error processing update for ") + update.symbol + ": " + e.what());
            } catch (...) {
                LOG_ERROR(std::string("Unknown error processing update for ") + update.symbol);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in processUpdates: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error in processUpdates");
    }
}

void ArbitrageEngine::detectArbitrage() {
    static int check_count = 0;
    check_count++;
    
    try {
        int opportunities_found = 0;
        
        for (const auto& arb_pair : ARB_PAIRS) {
            if (arb_pair == "ARB/USDT") {
                continue;
            }
            
            try {
                auto paths = router_->generateAllPaths(arb_pair);
                
                for (const auto& path : paths) {
                    try {
                        auto opportunity = checkPath(path, arb_pair);
                        if (opportunity.has_value()) {
                            opportunities_found++;
                            Logger::logArbitrageSignal(
                                opportunity->trade_sequence,
                                opportunity->profit_percentage,
                                opportunity->price_levels,
                                opportunity->max_tradable_amount);
                        }
                    } catch (const std::exception& e) {
                        LOG_ERROR(std::string("Error checking path: ") + e.what());
                    } catch (...) {
                        LOG_ERROR("Unknown error checking path");
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error generating paths for ") + arb_pair + ": " + e.what());
            } catch (...) {
                LOG_ERROR(std::string("Unknown error generating paths for ") + arb_pair);
            }
        }
        
        // Log detection status every 10 checks (to reduce spam)
        if (check_count % 10 == 0) {
            LOG_INFO("Arbitrage check #" + std::to_string(check_count) + 
                " completed. Opportunities found: " + std::to_string(opportunities_found));
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in detectArbitrage: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error in detectArbitrage");
    }
}

std::optional<ArbitrageOpportunity> ArbitrageEngine::checkPath(
    const TradePath& path,
    const std::string& arb_pair) const {
    
    if (path.steps.empty()) {
        return std::nullopt;
    }
    
    std::optional<double> buy_price = price_calculator_->calculateImpliedUSDTPrice(
        arb_pair, true);
    
    if (!buy_price.has_value()) {
        return std::nullopt;
    }
    
    std::optional<double> sell_price = price_calculator_->getDirectUSDTPrice(
        "ARB/USDT", false);
    
    if (!sell_price.has_value()) {
        return std::nullopt;
    }
    
    double profit_percentage = ((sell_price.value() - buy_price.value()) 
                                / buy_price.value()) * 100.0;
    
    if (profit_percentage < PROFIT_THRESHOLD) {
        return std::nullopt;
    }
    
    ArbitrageOpportunity opp;
    opp.trade_sequence = path.description;
    opp.profit_percentage = profit_percentage;
    opp.price_levels = formatPriceLevels(path);
    opp.max_tradable_amount = calculateMaxTradableAmount(path);
    opp.arb_pair = arb_pair;
    
    return opp;
}

double ArbitrageEngine::calculateMaxTradableAmount(
    const TradePath& path) const {
    
    if (path.steps.empty()) {
        return 0.0;
    }
    
    try {
        double max_amount = std::numeric_limits<double>::max();
        
        for (size_t i = 0; i < path.steps.size(); ++i) {
            auto it = order_books_.find(path.steps[i]);
            if (it == order_books_.end() || !it->second) {
                return 0.0;
            }
            
            try {
                auto levels = it->second->getTopNLevels(
                    ORDER_BOOK_DEPTH, !path.is_buy[i]);
                
                double available = 0.0;
                for (const auto& level : levels) {
                    available += level.quantity;
                }
                
                max_amount = std::min(max_amount, available);
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error getting levels for ") + path.steps[i] + ": " + e.what());
                return 0.0;
            } catch (...) {
                LOG_ERROR(std::string("Unknown error getting levels for ") + path.steps[i]);
                return 0.0;
            }
        }
        
        return max_amount;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in calculateMaxTradableAmount: ") + e.what());
        return 0.0;
    } catch (...) {
        LOG_ERROR("Unknown error in calculateMaxTradableAmount");
        return 0.0;
    }
}

std::string ArbitrageEngine::formatPriceLevels(
    const TradePath& path) const {
    
    std::stringstream ss;
    
    try {
        for (size_t i = 0; i < path.steps.size(); ++i) {
            auto it = order_books_.find(path.steps[i]);
            if (it != order_books_.end() && it->second) {
                try {
                    double price = path.is_buy[i] 
                        ? it->second->getBestAsk()
                        : it->second->getBestBid();
                    
                    ss << path.steps[i] << ":" 
                       << std::fixed << std::setprecision(8) << price;
                    
                    if (i < path.steps.size() - 1) {
                        ss << ", ";
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR(std::string("Error getting price for ") + path.steps[i] + ": " + e.what());
                } catch (...) {
                    LOG_ERROR(std::string("Unknown error getting price for ") + path.steps[i]);
                }
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in formatPriceLevels: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error in formatPriceLevels");
    }
    
    return ss.str();
}

