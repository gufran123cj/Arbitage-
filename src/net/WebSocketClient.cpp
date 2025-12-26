#include "WebSocketClient.hpp"
#include <iostream>
#include <openssl/ssl.h>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

WebSocketClient::WebSocketClient(const std::string& stream)
    : stream_(stream) {}

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
    try {
        net::io_context ioc;
        ssl::context ctx{ssl::context::tlsv12_client};

        ctx.set_default_verify_paths();

        tcp::resolver resolver{ioc};
        ws::stream<ssl::stream<tcp::socket>> ws{ioc, ctx};

        auto results = resolver.resolve("stream.binance.com", "443");
        net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

        SSL_set_tlsext_host_name(ws.next_layer().native_handle(), "stream.binance.com");

        ws.next_layer().handshake(ssl::stream_base::client);

        ws.handshake("stream.binance.com", "/ws/" + stream_);

        std::cout << "[WS] Connected to " << stream_ << std::endl;

        while (running_) {
            beast::flat_buffer buffer;
            ws.read(buffer);

            std::string msg = boost::beast::buffers_to_string(buffer.data());
            std::cout << msg << std::endl;
        }

        ws.close(ws::close_code::normal);
    }
    catch (const std::exception& e) {
        std::cerr << "[WS ERROR] " << e.what() << std::endl;
    }
}
