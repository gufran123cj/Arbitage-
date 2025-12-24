#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include "../core/MarketUpdate.hpp"
#include "../util/ConcurrentQueue.hpp"
#include "../core/OrderBook.hpp"
#include "../util/Logger.hpp"

class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient&) = delete;
    WebSocketClient& operator=(const WebSocketClient&) = delete;

    void start();
    void stop();
    
    void setUpdateQueue(
        std::shared_ptr<ConcurrentQueue<MarketUpdate>> queue);

private:
    static const std::vector<std::string> SUBSCRIBE_PAIRS;
    static const std::string BINANCE_WS_BASE_URL;
    static const std::string BINANCE_WS_STREAM_URL;
    static const std::string BINANCE_REST_API_BASE;

    std::atomic<bool> running_;
    std::thread client_thread_;

    std::shared_ptr<ConcurrentQueue<MarketUpdate>> update_queue_;

    void clientLoop();
    void simulateWebSocketFeed();
    
    // Real Binance API methods
    bool fetchOrderBookSnapshot(const std::string& symbol, MarketUpdate& update);
    std::string httpGet(const std::string& url);
    MarketUpdate parsePartialBookDepthJson(const std::string& symbol, const std::string& json_data);
    std::vector<PriceLevel> parsePriceLevelsArray(const std::string& json_array);
    
    std::string buildWebSocketUrl() const;
    std::string convertSymbolToBinanceFormat(const std::string& symbol) const;
    
    MarketUpdate parseMarketData(
        const std::string& symbol,
        const std::string& json_data) const;
    
    std::vector<PriceLevel> parsePriceLevels(
        const std::string& json_array) const;
};

