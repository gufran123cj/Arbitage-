#include "PriceCalculator.hpp"
#include "OrderBook.hpp"
#include <algorithm>
#include <cmath>
#include <optional>

constexpr double EPSILON = 1e-9;

PriceCalculator::PriceCalculator() {
}

void PriceCalculator::registerOrderBook(
    const std::string& symbol,
    std::shared_ptr<OrderBook> order_book) {
    
    order_books_[symbol] = order_book;
}

std::optional<double> PriceCalculator::calculateImpliedUSDTPrice(
    const std::string& arb_pair,
    bool is_buy) const {
    
    if (arb_pair == "ARB/USDT") {
        return getDirectUSDTPrice("ARB/USDT", is_buy);
    }
    
    std::string quote_currency = extractQuoteCurrency(arb_pair);
    
    std::optional<double> arb_price = getPrice(arb_pair, is_buy);
    if (!arb_price.has_value()) {
        return std::nullopt;
    }
    
    if (quote_currency == "USDT") {
        return arb_price;
    }
    
    std::optional<double> quote_to_usdt = getDirectUSDTPrice(
        quote_currency + "/USDT", is_buy);
    
    if (quote_to_usdt.has_value()) {
        return arb_price.value() * quote_to_usdt.value();
    }
    
    if (quote_currency == "BTC") {
        quote_to_usdt = getDirectUSDTPrice("BTC/USDT", is_buy);
        if (quote_to_usdt.has_value()) {
            return arb_price.value() * quote_to_usdt.value();
        }
    }
    
    if (quote_currency == "ETH") {
        quote_to_usdt = getDirectUSDTPrice("ETH/USDT", is_buy);
        if (quote_to_usdt.has_value()) {
            return arb_price.value() * quote_to_usdt.value();
        }
    }
    
    std::optional<double> via_btc = calculateViaBTC(quote_currency, is_buy);
    if (via_btc.has_value()) {
        return arb_price.value() * via_btc.value();
    }
    
    std::optional<double> via_eth = calculateViaETH(quote_currency, is_buy);
    if (via_eth.has_value()) {
        return arb_price.value() * via_eth.value();
    }
    
    return std::nullopt;
}

std::optional<double> PriceCalculator::getDirectUSDTPrice(
    const std::string& pair,
    bool is_buy) const {
    
    return getPrice(pair, is_buy);
}

std::optional<double> PriceCalculator::getPrice(
    const std::string& symbol,
    bool is_buy) const {
    
    auto it = order_books_.find(symbol);
    if (it == order_books_.end()) {
        return std::nullopt;
    }
    
    auto order_book = it->second;
    if (!order_book->isValid()) {
        return std::nullopt;
    }
    
    if (is_buy) {
        return order_book->getBestAsk();
    } else {
        return order_book->getBestBid();
    }
}

std::optional<double> PriceCalculator::calculateViaBTC(
    const std::string& quote_currency,
    bool is_buy) const {
    
    std::optional<double> quote_to_btc = getPrice(
        quote_currency + "/BTC", is_buy);
    
    if (!quote_to_btc.has_value()) {
        return std::nullopt;
    }
    
    std::optional<double> btc_to_usdt = getPrice("BTC/USDT", is_buy);
    if (!btc_to_usdt.has_value()) {
        return std::nullopt;
    }
    
    return quote_to_btc.value() * btc_to_usdt.value();
}

std::optional<double> PriceCalculator::calculateViaETH(
    const std::string& quote_currency,
    bool is_buy) const {
    
    std::optional<double> quote_to_eth = getPrice(
        quote_currency + "/ETH", is_buy);
    
    if (!quote_to_eth.has_value()) {
        return std::nullopt;
    }
    
    std::optional<double> eth_to_usdt = getPrice("ETH/USDT", is_buy);
    if (!eth_to_usdt.has_value()) {
        return std::nullopt;
    }
    
    return quote_to_eth.value() * eth_to_usdt.value();
}

std::string PriceCalculator::extractQuoteCurrency(
    const std::string& pair) const {
    
    size_t pos = pair.find('/');
    if (pos == std::string::npos) {
        return "";
    }
    return pair.substr(pos + 1);
}

