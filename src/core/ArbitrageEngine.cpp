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
    
    // Update timestamp if we have best bid/ask
    if (!update.bids.empty() && !update.asks.empty()) {
        it->second->updateTop(
            update.bids[0].price, update.bids[0].quantity,
            update.asks[0].price, update.asks[0].quantity,
            update.timestamp_ms);
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
        
        // Adım 5: İlk doğrulama - Tek rota testi (ARB/BTC + BTC/USDT)
        // Bu test her 50 check'te bir çalışsın (spam önlemek için)
        if (check_count % 50 == 1) {
            testSingleRoute();
        }
        
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

void ArbitrageEngine::testSingleRoute() const {
    // Adım 5: Tek rota testi - ARB/BTC + BTC/USDT → implied ARB/USDT
    // Formüller:
    // cost_usdt = ask(ARB/BTC) * ask(BTC/USDT)
    // final_usdt = bid(ARB/USDT)
    // profit_pct = (final_usdt / cost_usdt - 1) * 100
    
    auto arb_btc_it = order_books_.find("ARB/BTC");
    auto btc_usdt_it = order_books_.find("BTC/USDT");
    auto arb_usdt_it = order_books_.find("ARB/USDT");
    
    if (arb_btc_it == order_books_.end() || !arb_btc_it->second || !arb_btc_it->second->isValid()) {
        return;
    }
    if (btc_usdt_it == order_books_.end() || !btc_usdt_it->second || !btc_usdt_it->second->isValid()) {
        return;
    }
    if (arb_usdt_it == order_books_.end() || !arb_usdt_it->second || !arb_usdt_it->second->isValid()) {
        return;
    }
    
    // Data freshness check
    if (!arb_btc_it->second->isFresh(500) || 
        !btc_usdt_it->second->isFresh(500) || 
        !arb_usdt_it->second->isFresh(500)) {
        LOG_WARN("SKIP single route test - stale data");
        return;
    }
    
    double ask_arb_btc = arb_btc_it->second->getBestAsk();
    double ask_btc_usdt = btc_usdt_it->second->getBestAsk();
    double bid_arb_usdt = arb_usdt_it->second->getBestBid();
    
    if (ask_arb_btc <= 0.0 || ask_btc_usdt <= 0.0 || bid_arb_usdt <= 0.0) {
        return;
    }
    
    double cost_usdt = ask_arb_btc * ask_btc_usdt;
    double final_usdt = bid_arb_usdt;
    double profit_pct = ((final_usdt / cost_usdt) - 1.0) * 100.0;
    
    // Log test results
    LOG_INFO("Single Route Test (ARB/BTC + BTC/USDT):");
    LOG_INFO("  cost_usdt = " + std::to_string(cost_usdt));
    LOG_INFO("  final_usdt = " + std::to_string(final_usdt));
    LOG_INFO("  profit_pct = " + std::to_string(profit_pct) + "%");
}

std::optional<ArbitrageOpportunity> ArbitrageEngine::checkPath(
    const TradePath& path,
    const std::string& arb_pair) const {
    
    if (path.steps.empty()) {
        return std::nullopt;
    }
    
    // Adım 4: Data Freshness Gate (PDF gereksinimi)
    // Her route için: now - last_update_ms <= 500ms olması şart
    constexpr uint64_t MAX_DATA_AGE_MS = 500;
    
    for (const auto& step_pair : path.steps) {
        auto it = order_books_.find(step_pair);
        if (it == order_books_.end() || !it->second) {
            return std::nullopt;
        }
        
        if (!it->second->isFresh(MAX_DATA_AGE_MS)) {
            // Log stale data skip
            LOG_WARN("SKIP route " + path.description + " - stale data for " + step_pair);
            return std::nullopt;
        }
    }
    
    // ARB/USDT de kontrol et
    auto arb_usdt_it = order_books_.find("ARB/USDT");
    if (arb_usdt_it == order_books_.end() || !arb_usdt_it->second) {
        return std::nullopt;
    }
    if (!arb_usdt_it->second->isFresh(MAX_DATA_AGE_MS)) {
        LOG_WARN("SKIP route " + path.description + " - stale data for ARB/USDT");
        return std::nullopt;
    }
    
    // PDF'ye göre triangular arbitrage: "Buy ARB with EUR → Sell ARB for BTC → Sell BTC for USDT"
    // Path: ARB/EUR → ARB/BTC → BTC/USDT
    // 
    // Cost calculation: Her adımda ASK fiyatlarını kullan (almak için ödenen)
    // Final calculation: Son adımda BID fiyatı kullan (satmak için alınan)
    
    // İlk adım: ARB almak için ödenen fiyat (ASK)
    // Örnek: ARB/EUR'den ARB al, ASK(ARB/EUR) EUR öde
    std::string first_pair = path.steps[0];
    auto first_it = order_books_.find(first_pair);
    if (first_it == order_books_.end() || !first_it->second || !first_it->second->isValid()) {
        return std::nullopt;
    }
    
    if (!path.is_buy[0]) {
        return std::nullopt; // İlk adım buy olmalı
    }
    
    double cost_per_arb_in_quote = first_it->second->getBestAsk();
    if (cost_per_arb_in_quote <= 0.0) {
        return std::nullopt;
    }
    
    // İlk adımın quote currency'sini bul (EUR, BTC, vs.)
    size_t slash_pos = first_pair.find('/');
    if (slash_pos == std::string::npos) {
        return std::nullopt;
    }
    std::string quote_currency = first_pair.substr(slash_pos + 1);
    
    // Quote currency'yi USDT'ye çevir (path'teki ara adımları kullan)
    double quote_to_usdt = 1.0;
    
    if (quote_currency == "USDT") {
        quote_to_usdt = 1.0;
    } else {
        // Path'teki adımları takip et
        // Eğer path'te quote_currency'den USDT'ye bir yol varsa onu kullan
        bool found_path = false;
        
        // Direct: X/USDT
        for (size_t i = 1; i < path.steps.size(); ++i) {
            if (path.steps[i] == quote_currency + "/USDT") {
                auto it = order_books_.find(path.steps[i]);
                if (it != order_books_.end() && it->second && it->second->isValid()) {
                    quote_to_usdt = it->second->getBestAsk(); // USDT almak için ASK
                    found_path = true;
                    break;
                }
            }
        }
        
        // Fallback: X/BTC → BTC/USDT veya path'teki diğer yollar
        if (!found_path) {
            // PriceCalculator'ı kullan (mevcut mantık)
            std::optional<double> implied = price_calculator_->calculateImpliedUSDTPrice(
                quote_currency + "/USDT", true);
            if (implied.has_value()) {
                quote_to_usdt = implied.value();
                found_path = true;
            }
        }
        
        if (!found_path) {
            return std::nullopt;
        }
    }
    
    // Cost in USDT: ARB almak için ödenen toplam USDT miktarı
    double cost_usdt = cost_per_arb_in_quote * quote_to_usdt;
    
    // Final: ARB/USDT'den ARB satarak aldığımız USDT (BID)
    auto arb_usdt_it = order_books_.find("ARB/USDT");
    if (arb_usdt_it == order_books_.end() || !arb_usdt_it->second || !arb_usdt_it->second->isValid()) {
        return std::nullopt;
    }
    
    double final_usdt = arb_usdt_it->second->getBestBid(); // ARB satmak için BID
    
    if (final_usdt <= 0.0 || cost_usdt <= 0.0) {
        return std::nullopt;
    }
    
    // PDF'ye göre: profit_pct = (final_usdt / cost_usdt - 1) * 100
    double profit_percentage = ((final_usdt / cost_usdt) - 1.0) * 100.0;
    
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

