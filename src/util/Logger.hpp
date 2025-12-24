#pragma once

#include <string>
#include <iostream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <chrono>

class Logger {
public:
    enum class Level {
        INFO,
        WARN,
        ERROR
    };

    static void log(Level level, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        std::string level_str;
        switch (level) {
            case Level::INFO:
                level_str = "INFO";
                break;
            case Level::WARN:
                level_str = "WARN";
                break;
            case Level::ERROR:
                level_str = "ERROR";
                break;
        }
        
        std::cout << "[" << ss.str() << "] [" << level_str << "] " 
                  << message << std::endl;
    }

    static void info(const std::string& message) {
        log(Level::INFO, message);
    }

    static void warn(const std::string& message) {
        log(Level::WARN, message);
    }

    static void error(const std::string& message) {
        log(Level::ERROR, message);
    }

    static void logArbitrageSignal(
        const std::string& trade_sequence,
        double profit_percentage,
        const std::string& price_levels,
        double max_tradable_amount) {
        
        std::stringstream ss;
        ss << "ARBITRAGE OPPORTUNITY DETECTED:\n";
        ss << "  Trade Sequence: " << trade_sequence << "\n";
        ss << "  Profit: " << std::fixed << std::setprecision(4) 
           << profit_percentage << "%\n";
        ss << "  Price Levels: " << price_levels << "\n";
        ss << "  Max Tradable Amount: " << std::setprecision(8) 
           << max_tradable_amount << " ARB";
        
        log(Level::INFO, ss.str());
    }

private:
    static std::mutex mutex_;
};

#define LOG_INFO(msg) Logger::info(msg)
#define LOG_WARN(msg) Logger::warn(msg)
#define LOG_ERROR(msg) Logger::error(msg)

