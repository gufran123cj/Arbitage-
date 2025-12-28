#pragma once

#include <string>
#include <optional>

// Simple JSON parser for Binance bookTicker messages
// Format: {"u":123,"s":"ARBUSDT","b":"0.19700000","B":"216197.40000000","a":"0.19710000","A":"12194.70000000"}

struct BookTickerData {
    std::string symbol;
    double bid_price;
    double bid_qty;
    double ask_price;
    double ask_qty;
    bool valid;

    BookTickerData() : bid_price(0.0), bid_qty(0.0), ask_price(0.0), ask_qty(0.0), valid(false) {}
};

class JsonParser {
public:
    // Parse bookTicker JSON message
    static BookTickerData parseBookTicker(const std::string& json);
    
    // Normalize symbol: "ARBUSDT" -> "ARB/USDT"
    static std::string normalizeSymbol(const std::string& symbol);

private:
    // Extract string field from JSON
    static std::optional<std::string> extractStringField(const std::string& json, const std::string& field_name);
    
    // Extract numeric field from JSON and convert to double
    static std::optional<double> extractNumericField(const std::string& json, const std::string& field_name);
    
    // Safe string to double conversion
    static double stringToDouble(const std::string& str);
};
