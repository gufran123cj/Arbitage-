#include "ArbitrageLogger.hpp"

ArbitrageLogger::ArbitrageLogger() {
    // Constructor - can be used for initialization if needed
}

ArbitrageLogger::~ArbitrageLogger() {
    // Destructor - cleanup if needed
}

void ArbitrageLogger::logOpportunity(const ArbitrageOpportunity& opp) {
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    // Generate filename
    std::string filename = generateFilename();
    
    // Get current timestamp in milliseconds for JSON
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // Create JSON content (manual JSON construction - no external library)
    std::ostringstream json_oss;
    json_oss << std::fixed << std::setprecision(8);
    
    json_oss << "{\n";
    json_oss << "  \"timestamp_ms\": " << now_ms << ",\n";
    json_oss << "  \"timestamp\": \"" << formatTimestamp(tm) << "\",\n";
    json_oss << "  \"direction\": " << opp.direction << ",\n";
    json_oss << "  \"route_name\": \"" << opp.route_name << "\",\n";
    json_oss << "  \"trade_sequence\": \"" << opp.trade_sequence << "\",\n";
    json_oss << "  \"profit_percent\": " << opp.profit_percent << ",\n";
    json_oss << "  \"max_tradable_amount\": " << opp.max_tradable_amount << ",\n";
    json_oss << "  \"max_tradable_currency\": \"" << opp.max_tradable_currency << "\",\n";
    json_oss << "  \"prices\": {\n";
    json_oss << "    \"arb_usdt_bid\": " << opp.arb_usdt_bid << ",\n";
    json_oss << "    \"arb_usdt_ask\": " << opp.arb_usdt_ask << ",\n";
    json_oss << "    \"arb_other_bid\": " << opp.arb_other_bid << ",\n";
    json_oss << "    \"arb_other_ask\": " << opp.arb_other_ask << ",\n";
    json_oss << "    \"other_usdt_bid\": " << opp.other_usdt_bid << ",\n";
    json_oss << "    \"other_usdt_ask\": " << opp.other_usdt_ask << "\n";
    json_oss << "  }\n";
    json_oss << "}\n";
    
    // Write to file
    try {
        std::ofstream file(filename, std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << json_oss.str();
            file.close();
        }
    } catch (const std::exception&) {
        // Silent failure - don't spam console
    }
}

std::string ArbitrageLogger::generateFilename() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::localtime(&time_t);
    
    std::ostringstream oss;
    oss << "arbitrage_";
    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    oss << ".json";
    return oss.str();
}

std::string ArbitrageLogger::formatTimestamp(const std::tm& tm) const {
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
