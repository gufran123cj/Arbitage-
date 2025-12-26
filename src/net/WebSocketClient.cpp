#include "WebSocketClient.hpp"
#include <random>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

// Binance API endpoints
const std::string WebSocketClient::BINANCE_WS_BASE_URL = "wss://stream.binance.com:9443/ws/";
const std::string WebSocketClient::BINANCE_WS_STREAM_URL = "wss://stream.binance.com:9443/stream?streams=";
const std::string WebSocketClient::BINANCE_REST_API_BASE = "https://api.binance.com/api/v3/";

const std::vector<std::string> WebSocketClient::SUBSCRIBE_PAIRS = {
    "ARB/USDT", "ARB/FDUSD", "ARB/USDC", "ARB/TUSD",
    "ARB/BTC", "ARB/ETH", "ARB/TRY", "ARB/EUR",
    "BTC/USDT", "ETH/USDT", "EUR/USDT", "TRY/USDT",
    "EUR/BTC", "TRY/BTC", "FDUSD/USDT", "USDC/USDT", "TUSD/USDT"
};

WebSocketClient::WebSocketClient() : running_(false) {
}

WebSocketClient::~WebSocketClient() {
    stop();
}

void WebSocketClient::start() {
    if (running_.load()) {
        return;
    }
    
    running_.store(true);
    client_thread_ = std::thread(&WebSocketClient::clientLoop, this);
    LOG_INFO("WebSocket client started (simulated mode)");
}

void WebSocketClient::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_.store(false);
    if (client_thread_.joinable()) {
        client_thread_.join();
    }
}

void WebSocketClient::setUpdateQueue(
    std::shared_ptr<ConcurrentQueue<MarketUpdate>> queue) {
    
    update_queue_ = queue;
}

void WebSocketClient::clientLoop() {
    LOG_INFO("WebSocketClient loop started");
    LOG_INFO("Using periodic snapshot refresh from Binance REST API (every 5 seconds)");
    
    // Track last snapshot refresh time
    auto last_snapshot_time = std::chrono::steady_clock::now();
    constexpr auto SNAPSHOT_REFRESH_INTERVAL = std::chrono::seconds(5);
    
    try {
        while (running_.load()) {
            try {
                auto now = std::chrono::steady_clock::now();
                auto time_since_last_snapshot = std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_snapshot_time);
                
                // Refresh snapshots every 5 seconds
                if (time_since_last_snapshot >= SNAPSHOT_REFRESH_INTERVAL) {
                    LOG_INFO("Refreshing order book snapshots from Binance REST API...");
                    
                    int success_count = 0;
                    int fail_count = 0;
                    
                    for (const auto& symbol : SUBSCRIBE_PAIRS) {
                        try {
                            MarketUpdate update;
                            if (fetchOrderBookSnapshot(symbol, update)) {
                                if (update_queue_) {
                                    update_queue_->push(update);
                                    success_count++;
                                    
                                    // Log market data to file for all pairs
                                    if (!update.bids.empty() && !update.asks.empty()) {
                                        Logger::logMarketData(
                                            symbol,
                                            update.bids[0].price,
                                            update.asks[0].price,
                                            update.bids[0].quantity,
                                            update.asks[0].quantity
                                        );
                                        
                                        // Also log to console for ARB pairs
                                        if (symbol.find("ARB/") == 0) {
                                            double spread = ((update.asks[0].price - update.bids[0].price) / update.bids[0].price) * 100.0;
                                            LOG_INFO("Snapshot: " + symbol + 
                                                " Bid: " + std::to_string(update.bids[0].price) + 
                                                " Ask: " + std::to_string(update.asks[0].price) +
                                                " (Spread: " + std::to_string(spread) + "%)");
                                        }
                                    }
                                }
                            } else {
                                fail_count++;
                                LOG_WARN("Failed to fetch snapshot for " + symbol);
                            }
                            // Small delay between requests to avoid rate limiting
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        } catch (const std::exception& e) {
                            fail_count++;
                            LOG_ERROR(std::string("Error fetching snapshot for ") + symbol + ": " + e.what());
                        } catch (...) {
                            fail_count++;
                            LOG_ERROR(std::string("Unknown error fetching snapshot for ") + symbol);
                        }
                    }
                    
                    LOG_INFO("Snapshot refresh completed: " + std::to_string(success_count) + 
                        " success, " + std::to_string(fail_count) + " failed");
                    
                    last_snapshot_time = now;
                }
                
                // Small sleep to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error in WebSocket loop: ") + e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            } catch (...) {
                LOG_ERROR("Unknown error in WebSocket loop");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        }
    } catch (...) {
        LOG_ERROR("Fatal error in WebSocket loop");
    }
    LOG_INFO("WebSocketClient loop stopped");
}

void WebSocketClient::simulateWebSocketFeed() {
    if (!update_queue_) {
        LOG_WARN("Update queue is null in simulateWebSocketFeed");
        return;
    }
    
    try {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<double> price_dist(0.5, 2.0);
        static std::uniform_real_distribution<double> qty_dist(100.0, 1000.0);
        
        for (const auto& symbol : SUBSCRIBE_PAIRS) {
            try {
                MarketUpdate update;
                update.symbol = symbol;
                update.is_snapshot = false;
                
                double base_price = price_dist(gen);
                
                for (int i = 0; i < 5; ++i) {
                    double bid_price = base_price * (1.0 - 0.001 * i);
                    double ask_price = base_price * (1.0 + 0.001 * i);
                    double qty = qty_dist(gen);
                    
                    update.bids.push_back(PriceLevel(bid_price, qty));
                    update.asks.push_back(PriceLevel(ask_price, qty));
                }
                
                update_queue_->push(update);
            } catch (const std::exception& e) {
                LOG_ERROR(std::string("Error creating update for ") + symbol + ": " + e.what());
            } catch (...) {
                LOG_ERROR(std::string("Unknown error creating update for ") + symbol);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error in simulateWebSocketFeed: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error in simulateWebSocketFeed");
    }
}

    MarketUpdate WebSocketClient::parseMarketData(
        const std::string& symbol,
        const std::string& /* json_data */) const {
    
    MarketUpdate update;
    update.symbol = symbol;
    update.is_snapshot = false;
    
    return update;
}

std::string WebSocketClient::buildWebSocketUrl() const {
    // Binance WebSocket URL format:
    // Single stream: wss://stream.binance.com:9443/ws/arbusdt@depth20@100ms
    // Multiple streams: wss://stream.binance.com:9443/stream?streams=arbusdt@depth20@100ms/btcusdt@depth20@100ms
    
    std::stringstream url_stream;
    
    if (SUBSCRIBE_PAIRS.size() == 1) {
        // Single stream
        std::string symbol = convertSymbolToBinanceFormat(SUBSCRIBE_PAIRS[0]);
        url_stream << BINANCE_WS_BASE_URL << symbol << "@depth20@100ms";
    } else {
        // Multiple streams
        url_stream << BINANCE_WS_STREAM_URL;
        for (size_t i = 0; i < SUBSCRIBE_PAIRS.size(); ++i) {
            std::string symbol = convertSymbolToBinanceFormat(SUBSCRIBE_PAIRS[i]);
            url_stream << symbol << "@depth20@100ms";
            if (i < SUBSCRIBE_PAIRS.size() - 1) {
                url_stream << "/";
            }
        }
    }
    
    return url_stream.str();
}

std::string WebSocketClient::convertSymbolToBinanceFormat(const std::string& symbol) const {
    // Convert "ARB/USDT" to "arbusdt" (lowercase, remove /)
    std::string result = symbol;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    result.erase(std::remove(result.begin(), result.end(), '/'), result.end());
    return result;
}

std::vector<PriceLevel> WebSocketClient::parsePriceLevels(
        const std::string& /* json_array */) const {
    
    std::vector<PriceLevel> levels;
    return levels;
}

#ifdef _WIN32
std::string WebSocketClient::httpGet(const std::string& url) {
    std::string result;
    
    // Simple URL parsing for https://api.binance.com/api/v3/depth?symbol=...
    // Extract host and path
    size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos) {
        LOG_ERROR("Invalid URL format: " + url);
        return result;
    }
    
    size_t host_start = scheme_end + 3;
    size_t path_start = url.find('/', host_start);
    if (path_start == std::string::npos) {
        LOG_ERROR("Invalid URL format: " + url);
        return result;
    }
    
    std::string host = url.substr(host_start, path_start - host_start);
    std::string path = url.substr(path_start);
    
    // Convert to wide string for WinHTTP
    std::wstring host_w(host.begin(), host.end());
    std::wstring path_w(path.begin(), path.end());
    
    // Open session
    HINTERNET hSession = WinHttpOpen(L"ARB-Arbitrage-Engine/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    
    if (!hSession) {
        DWORD error = GetLastError();
        LOG_ERROR("Failed to open WinHTTP session (Error: " + std::to_string(error) + ")");
        return result;
    }
    
    // Set timeouts (30 seconds for connect, send, receive)
    DWORD timeout = 30000; // 30 seconds in milliseconds
    WinHttpSetTimeouts(hSession, timeout, timeout, timeout, timeout);
    
    // Connect to host (HTTPS uses port 443)
    HINTERNET hConnect = WinHttpConnect(hSession, host_w.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    
    if (!hConnect) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hSession);
        LOG_ERROR("Failed to connect to host: " + host + " (Error: " + std::to_string(error) + ")");
        return result;
    }
    
    // Open request
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path_w.c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    
    if (!hRequest) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LOG_ERROR("Failed to open request (Error: " + std::to_string(error) + ")");
        return result;
    }
    
    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LOG_ERROR("Failed to send request to " + host + path + " (Error: " + std::to_string(error) + ")");
        return result;
    }
    
    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD error = GetLastError();
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        LOG_ERROR("Failed to receive response from " + host + path + " (Error: " + std::to_string(error) + ")");
        return result;
    }
    
    // Read data
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;
    char buffer[4096] = {0};
    
    do {
        if (!WinHttpQueryDataAvailable(hRequest, &bytes_available)) {
            break;
        }
        
        if (bytes_available == 0) {
            break;
        }
        
        if (!WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytes_read)) {
            break;
        }
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            result += buffer;
        }
    } while (bytes_read > 0);
    
    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return result;
}
#else
std::string WebSocketClient::httpGet(const std::string& url) {
    // For non-Windows platforms, use curl or similar
    // For now, return empty (can be extended with libcurl)
    LOG_WARN("HTTP GET not implemented for non-Windows platforms");
    return std::string();
}
#endif

bool WebSocketClient::fetchOrderBookSnapshot(const std::string& symbol, MarketUpdate& update) {
    try {
        std::string binance_symbol = convertSymbolToBinanceFormat(symbol);
        std::string url = BINANCE_REST_API_BASE + "depth?symbol=" + binance_symbol + "&limit=20";
        
        std::string json_response = httpGet(url);
        
        if (json_response.empty()) {
            LOG_ERROR("Empty response from Binance API for " + symbol);
            return false;
        }
        
        update = parsePartialBookDepthJson(symbol, json_response);
        update.is_snapshot = true;
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception in fetchOrderBookSnapshot: ") + e.what());
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception in fetchOrderBookSnapshot");
        return false;
    }
}

MarketUpdate WebSocketClient::parsePartialBookDepthJson(const std::string& symbol, const std::string& json_data) {
    MarketUpdate update;
    update.symbol = symbol;
    update.is_snapshot = true;
    
    // Set timestamp (current time in milliseconds)
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    update.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // Simple JSON parsing for Binance depth response
    // Format: {"lastUpdateId":123,"bids":[["price","qty"],...],"asks":[["price","qty"],...]}
    
    try {
        // Find bids array
        size_t bids_start = json_data.find("\"bids\":[");
        size_t asks_start = json_data.find("\"asks\":[");
        
        if (bids_start != std::string::npos && asks_start != std::string::npos) {
            // Extract bids
            size_t bids_array_start = json_data.find('[', bids_start);
            size_t bids_array_end = json_data.find(']', bids_array_start);
            
            if (bids_array_start != std::string::npos && bids_array_end != std::string::npos) {
                std::string bids_str = json_data.substr(bids_array_start + 1, bids_array_end - bids_array_start - 1);
                update.bids = parsePriceLevelsArray(bids_str);
            }
            
            // Extract asks
            size_t asks_array_start = json_data.find('[', asks_start);
            size_t asks_array_end = json_data.find(']', asks_array_start);
            
            if (asks_array_start != std::string::npos && asks_array_end != std::string::npos) {
                std::string asks_str = json_data.substr(asks_array_start + 1, asks_array_end - asks_array_start - 1);
                update.asks = parsePriceLevelsArray(asks_str);
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error parsing JSON: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error parsing JSON");
    }
    
    return update;
}

std::vector<PriceLevel> WebSocketClient::parsePriceLevelsArray(const std::string& json_array) {
    std::vector<PriceLevel> levels;
    
    try {
        // Parse array of ["price","qty"] pairs
        size_t pos = 0;
        while (pos < json_array.length()) {
            // Find next [
            size_t array_start = json_array.find('[', pos);
            if (array_start == std::string::npos) {
                break;
            }
            
            // Find matching ]
            size_t array_end = json_array.find(']', array_start);
            if (array_end == std::string::npos) {
                break;
            }
            
            // Extract price and quantity
            std::string pair_str = json_array.substr(array_start + 1, array_end - array_start - 1);
            
            // Find price (first quoted string)
            size_t price_start = pair_str.find('"');
            if (price_start == std::string::npos) continue;
            size_t price_end = pair_str.find('"', price_start + 1);
            if (price_end == std::string::npos) continue;
            
            std::string price_str = pair_str.substr(price_start + 1, price_end - price_start - 1);
            
            // Find quantity (second quoted string)
            size_t qty_start = pair_str.find('"', price_end + 1);
            if (qty_start == std::string::npos) continue;
            size_t qty_end = pair_str.find('"', qty_start + 1);
            if (qty_end == std::string::npos) continue;
            
            std::string qty_str = pair_str.substr(qty_start + 1, qty_end - qty_start - 1);
            
            try {
                double price = std::stod(price_str);
                double quantity = std::stod(qty_str);
                levels.push_back(PriceLevel(price, quantity));
            } catch (...) {
                // Skip invalid price/qty
            }
            
            pos = array_end + 1;
        }
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error parsing price levels array: ") + e.what());
    } catch (...) {
        LOG_ERROR("Unknown error parsing price levels array");
    }
    
    return levels;
}

