#pragma once

#include <string>
#include <vector>

struct TradePath {
    std::vector<std::string> steps;
    std::vector<bool> is_buy;
    std::string description;

    TradePath() = default;
    
    TradePath(const std::vector<std::string>& s,
              const std::vector<bool>& buy,
              const std::string& desc)
        : steps(s), is_buy(buy), description(desc) {}
};

class Router {
public:
    Router();
    ~Router() = default;

    std::vector<TradePath> generateDirectPaths(
        const std::string& arb_pair) const;

    std::vector<TradePath> generateTwoLegPaths(
        const std::string& arb_pair) const;

    std::vector<TradePath> generateTriangularPaths(
        const std::string& arb_pair) const;

    std::vector<TradePath> generateMultiLegPaths(
        const std::string& arb_pair) const;

    std::vector<TradePath> generateAllPaths(
        const std::string& arb_pair) const;

private:
    static const std::vector<std::string> ARB_PAIRS;
    static const std::vector<std::string> INTERMEDIATE_PAIRS;
};

