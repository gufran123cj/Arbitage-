#include "src/net/WebSocketClient.hpp"
#include <chrono>
#include <thread>

int main() {
    WebSocketClient ws("arbusdt@bookTicker");
    ws.start();

    std::this_thread::sleep_for(std::chrono::seconds(10));
    ws.stop();
}
