#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include "src/core/MarketUpdate.hpp"
#include "src/net/WebSocketClient.hpp"
#include "src/core/ArbitrageEngine.hpp"
#include "src/util/Logger.hpp"
#include "src/util/ConcurrentQueue.hpp"
#ifdef _WIN32
#include <windows.h>
#undef ERROR
#else
#include <csignal>
#endif

static std::atomic<bool> g_running{true};

#ifdef _WIN32
BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT) {
        g_running.store(false);
        LOG_INFO("Shutdown signal received...");
        return TRUE;
    }
    return FALSE;
}
#else
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running.store(false);
        LOG_INFO("Shutdown signal received...");
    }
}
#endif

int main() {
    try {
#ifdef _WIN32
        SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
        LOG_INFO("Windows signal handler registered");
#else
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        LOG_INFO("Unix signal handler registered");
#endif
        
        LOG_INFO("Starting ARB Arbitrage Engine");
        
        LOG_INFO("Creating update queue...");
        auto update_queue = std::make_shared<ConcurrentQueue<MarketUpdate>>();
        LOG_INFO("Update queue created");
        
        LOG_INFO("Creating WebSocket client...");
        WebSocketClient ws_client;
        LOG_INFO("WebSocket client created");
        
        LOG_INFO("Setting update queue for WebSocket client...");
        ws_client.setUpdateQueue(update_queue);
        LOG_INFO("Update queue set for WebSocket client");
        
        LOG_INFO("Creating ArbitrageEngine...");
        ArbitrageEngine engine;
        LOG_INFO("ArbitrageEngine created");
        
        LOG_INFO("Setting update queue for engine...");
        engine.setUpdateQueue(update_queue);
        LOG_INFO("Update queue set for engine");
        
        LOG_INFO("Starting WebSocket client...");
        ws_client.start();
        LOG_INFO("WebSocket client started");
        
        LOG_INFO("Starting ArbitrageEngine...");
        engine.start();
        LOG_INFO("ArbitrageEngine started");
        
        LOG_INFO("System running. Press Ctrl+C to stop...");
        
        auto last_status_time = std::chrono::steady_clock::now();
        const auto status_interval = std::chrono::seconds(5);
        
        while (g_running.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_status_time >= status_interval) {
                LOG_INFO("System is running... Monitoring arbitrage opportunities...");
                last_status_time = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        LOG_INFO("Shutdown signal received, stopping components...");
        engine.stop();
        LOG_INFO("ArbitrageEngine stopped");
        
        ws_client.stop();
        LOG_INFO("WebSocket client stopped");
        
        LOG_INFO("System stopped successfully");
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception caught: ") + e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown exception occurred");
        return 1;
    }
}

