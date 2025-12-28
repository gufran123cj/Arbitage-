#include "WebSocketClient.hpp"
#include "../core/MarketState.hpp"
#include "../util/JsonParser.hpp"
#include <iostream>
#include <openssl/ssl.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <chrono>
#include <thread>
#include <algorithm>

WebSocketClient::WebSocketClient(const std::string& stream, MarketState& market_state)
    : stream_(stream), market_state_(market_state) {}

WebSocketClient::~WebSocketClient() {
    stop();
}

void WebSocketClient::start() {
    running_ = true;
    thread_ = std::thread(&WebSocketClient::run, this);
}

void WebSocketClient::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void WebSocketClient::run() {
    const int MAX_RETRY_DELAY_MS = 30000; // Maximum 30 seconds
    int retry_delay_ms = 1000; // Start with 1 second
    
    while (running_) {
        bool connected = false;
        ws::stream<ssl::stream<tcp::socket>>* ws_ptr = nullptr;
        
        try {
            net::io_context ioc;
            ssl::context ctx{ssl::context::tlsv12_client};

            ctx.set_default_verify_paths();
            ctx.set_verify_mode(ssl::verify_none);

            tcp::resolver resolver{ioc};
            ws::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};
            ws_ptr = &ws;

            // Resolve and connect
            auto results = resolver.resolve("stream.binance.com", "443");
            net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

            SSL_set_tlsext_host_name(ws.next_layer().native_handle(), "stream.binance.com");

            // SSL handshake
            ws.next_layer().handshake(ssl::stream_base::client);

            // WebSocket handshake
            ws.handshake("stream.binance.com", "/ws/" + stream_);

            std::cout << "[WS] Connected to " << stream_ << std::endl;
            connected = true;
            retry_delay_ms = 1000; // Reset retry delay on successful connection

            // Read messages loop
            while (running_ && ws.is_open()) {
                try {
                    beast::flat_buffer buffer;
                    ws.read(buffer);

                    if (!running_) {
                        break;
                    }

                    std::string msg = boost::beast::buffers_to_string(buffer.data());
                    
                    // Parse JSON message
                    BookTickerData data = JsonParser::parseBookTicker(msg);
                    
                    if (data.valid) {
                        // Get current timestamp in milliseconds
                        auto now = std::chrono::system_clock::now();
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()).count();
                        
                        // Update MarketState
                        market_state_.get(data.symbol).update(
                            data.bid_price,
                            data.bid_qty,
                            data.ask_price,
                            data.ask_qty,
                            static_cast<int64_t>(ms)
                        );
                    }
                }
                catch (const beast::system_error& se) {
                    if (se.code() == beast::websocket::error::closed) {
                        std::cout << "[WS] Connection closed by server for " << stream_ << std::endl;
                        break; // Exit read loop, will reconnect
                    }
                    else if (se.code() == boost::asio::error::eof || 
                             se.code() == boost::asio::ssl::error::stream_truncated) {
                        std::cout << "[WS] Stream ended (EOF or truncated) for " << stream_ << std::endl;
                        break; // Exit read loop, will reconnect
                    }
                    else {
                        std::cerr << "[WS ERROR] Read error for " << stream_ << ": " << se.what() 
                                  << " (code: " << se.code() << ")" << std::endl;
                        break; // Exit read loop, will reconnect
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[WS ERROR] Exception during read for " << stream_ << ": " << e.what() << std::endl;
                    break; // Exit read loop, will reconnect
                }
            }

            // Close connection gracefully if still open
            if (ws_ptr && ws_ptr->is_open()) {
                try {
                    ws.close(ws::close_code::normal);
                }
                catch (...) {
                    // Ignore errors during close
                }
            }
        }
        catch (const beast::system_error& se) {
            std::cerr << "[WS ERROR] Connection error for " << stream_ << ": " << se.what() 
                      << " (code: " << se.code() << ")" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[WS ERROR] Exception for " << stream_ << ": " << e.what() << std::endl;
        }

        // If connection failed or lost, wait and retry
        if (running_ && !connected) {
            std::cout << "[WS] Reconnecting to " << stream_ << " in " << (retry_delay_ms / 1000.0) << " seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
            
            // Exponential backoff: double the delay, but cap at MAX_RETRY_DELAY_MS
            retry_delay_ms = std::min(retry_delay_ms * 2, MAX_RETRY_DELAY_MS);
        }
        else if (running_) {
            // Connection was lost, wait a bit before reconnecting
            std::cout << "[WS] Connection lost for " << stream_ << ". Reconnecting in " 
                      << (retry_delay_ms / 1000.0) << " seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
            
            // Reset retry delay to initial value for reconnection after successful connection
            retry_delay_ms = 1000;
        }
    }
    
    std::cout << "[WS] Stopped " << stream_ << std::endl;
}
