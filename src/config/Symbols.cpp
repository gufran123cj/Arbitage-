#include "Symbols.hpp"
#include <algorithm>
#include <cctype>

namespace Symbols {
    std::string toBinanceStream(const std::string& symbol) {
        std::string stream = symbol;
        
        // Remove '/' and convert to lowercase
        stream.erase(std::remove(stream.begin(), stream.end(), '/'), stream.end());
        std::transform(stream.begin(), stream.end(), stream.begin(), ::tolower);
        
        // Add @bookTicker suffix
        return stream + "@bookTicker";
    }
    
    std::vector<std::string> getAllSymbols() {
        std::vector<std::string> all;
        
        // Add all ARB pairs
        all.insert(all.end(), ARB_PAIRS.begin(), ARB_PAIRS.end());
        
        // Add cross pairs
        all.insert(all.end(), CROSS_PAIRS.begin(), CROSS_PAIRS.end());
        
        return all;
    }
}
