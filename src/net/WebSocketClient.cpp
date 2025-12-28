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
    ws::stream<ssl::stream<tcp::socket>>* ws_ptr = nullptr;
    
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_none); // Binance uses valid certs, but we skip verification for simplicity

        tcp::resolver resolver{ioc};
        ws::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};
        ws_ptr = &ws;

        auto results = resolver.resolve("stream.binance.com", "443");
        net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

        SSL_set_tlsext_host_name(ws.next_layer().native_handle(), "stream.binance.com");

        ws.next_layer().handshake(ssl::stream_base::client);

        ws.handshake("stream.binance.com", "/ws/" + stream_);

        std::cout << "[WS] Connected to " << stream_ << std::endl;

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
                    std::cout << "[WS] Connection closed by server" << std::endl;
                    break;
                }
                else if (se.code() == boost::asio::error::eof || 
                         se.code() == boost::asio::ssl::error::stream_truncated) {
                    std::cout << "[WS] Stream ended (EOF or truncated)" << std::endl;
                    break;
                }
                else {
                    std::cerr << "[WS ERROR] Read error: " << se.what() << " (code: " << se.code() << ")" << std::endl;
                    break;
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[WS ERROR] Exception during read: " << e.what() << std::endl;
                break;
            }
        }

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
        std::cerr << "[WS ERROR] System error: " << se.what() << " (code: " << se.code() << ")" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[WS ERROR] Exception: " << e.what() << std::endl;
    }
}
