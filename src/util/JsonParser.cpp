#include "JsonParser.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <vector>

BookTickerData JsonParser::parseBookTicker(const std::string& json) {
    BookTickerData data;
    
    // Extract symbol
    auto symbol_opt = extractStringField(json, "s");
    if (!symbol_opt.has_value()) {
        return data; // invalid
    }
    data.symbol = normalizeSymbol(symbol_opt.value());
    
    // Extract bid price
    auto bid_price_opt = extractNumericField(json, "b");
    if (!bid_price_opt.has_value()) {
        return data; // invalid
    }
    data.bid_price = bid_price_opt.value();
    
    // Extract bid quantity
    auto bid_qty_opt = extractNumericField(json, "B");
    if (!bid_qty_opt.has_value()) {
        return data; // invalid
    }
    data.bid_qty = bid_qty_opt.value();
    
    // Extract ask price
    auto ask_price_opt = extractNumericField(json, "a");
    if (!ask_price_opt.has_value()) {
        return data; // invalid
    }
    data.ask_price = ask_price_opt.value();
    
    // Extract ask quantity
    auto ask_qty_opt = extractNumericField(json, "A");
    if (!ask_qty_opt.has_value()) {
        return data; // invalid
    }
    data.ask_qty = ask_qty_opt.value();
    
    // Validate: prices and quantities must be positive
    if (data.bid_price <= 0.0 || data.ask_price <= 0.0 || 
        data.bid_qty <= 0.0 || data.ask_qty <= 0.0) {
        return data; // invalid
    }
    
    data.valid = true;
    return data;
}

std::string JsonParser::normalizeSymbol(const std::string& symbol) {
    // Common quote currencies to detect
    const std::vector<std::string> quote_currencies = {
        "USDT", "USDC", "FDUSD", "TUSD", "BTC", "ETH", "EUR", "TRY", "BNB", "BUSD"
    };
    
    // Try to find a quote currency match
    for (const auto& quote : quote_currencies) {
        if (symbol.size() >= quote.size()) {
            size_t pos = symbol.size() - quote.size();
            if (symbol.substr(pos) == quote) {
                std::string base = symbol.substr(0, pos);
                return base + "/" + quote;
            }
        }
    }
    
    // If no match found, return as-is (shouldn't happen with Binance symbols)
    return symbol;
}

std::optional<std::string> JsonParser::extractStringField(const std::string& json, const std::string& field_name) {
    // Look for: "field_name":"value"
    std::string pattern = "\"" + field_name + "\":\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    
    pos += pattern.length();
    size_t end_pos = json.find("\"", pos);
    if (end_pos == std::string::npos) {
        return std::nullopt;
    }
    
    return json.substr(pos, end_pos - pos);
}

std::optional<double> JsonParser::extractNumericField(const std::string& json, const std::string& field_name) {
    // Look for: "field_name":"123.456" or "field_name":123.456
    std::string pattern1 = "\"" + field_name + "\":\"";
    size_t pos = json.find(pattern1);
    
    if (pos != std::string::npos) {
        // String format: "field_name":"123.456"
        pos += pattern1.length();
        size_t end_pos = json.find("\"", pos);
        if (end_pos == std::string::npos) {
            return std::nullopt;
        }
        std::string value_str = json.substr(pos, end_pos - pos);
        return stringToDouble(value_str);
    }
    
    // Try numeric format: "field_name":123.456
    std::string pattern2 = "\"" + field_name + "\":";
    pos = json.find(pattern2);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    
    pos += pattern2.length();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }
    
    // Find end (comma, }, or whitespace)
    size_t end_pos = pos;
    while (end_pos < json.size() && 
           json[end_pos] != ',' && 
           json[end_pos] != '}' && 
           json[end_pos] != ' ' && 
           json[end_pos] != '\t' &&
           json[end_pos] != '\n') {
        ++end_pos;
    }
    
    if (end_pos == pos) {
        return std::nullopt;
    }
    
    std::string value_str = json.substr(pos, end_pos - pos);
    return stringToDouble(value_str);
}

double JsonParser::stringToDouble(const std::string& str) {
    try {
        return std::stod(str);
    }
    catch (const std::exception&) {
        return 0.0;
    }
}
