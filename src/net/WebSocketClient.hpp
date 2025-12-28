#pragma once

#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <atomic>
#include <string>
#include <thread>
#include <functional>

namespace net  = boost::asio;
namespace ssl  = net::ssl;
namespace beast = boost::beast;
namespace ws   = beast::websocket;
using tcp = net::ip::tcp;

// Forward declaration
class MarketState;

class WebSocketClient {
public:
    explicit WebSocketClient(const std::string& stream, MarketState& market_state);
    ~WebSocketClient();

    void start();
    void stop();

private:
    void run();

    std::string stream_;
    MarketState& market_state_;
    std::atomic<bool> running_{false};
    std::thread thread_;
};
