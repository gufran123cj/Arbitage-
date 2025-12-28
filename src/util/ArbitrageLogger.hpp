#pragma once

#include "src/core/ArbitrageDetector.hpp"
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>

class ArbitrageLogger {
public:
    ArbitrageLogger();
    ~ArbitrageLogger();
    
    // Log an arbitrage opportunity to JSON file
    void logOpportunity(const ArbitrageOpportunity& opp);
    
private:
    // Generate filename with timestamp
    std::string generateFilename() const;
    
    // Format timestamp string
    std::string formatTimestamp(const std::tm& tm) const;
};
