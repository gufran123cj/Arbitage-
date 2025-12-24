# Binance ARB Arbitrage Engine

A high-frequency trading system for detecting triangular arbitrage opportunities in Arbitrum (ARB) trading pairs on Binance.

## Architecture

The system is built with C++17 and follows a modular architecture:

### Core Components

- **OrderBook**: Thread-safe order book management with best bid/ask and top N levels support
- **PriceCalculator**: Calculates implied USDT prices for ARB pairs using intermediate currencies
- **Router**: Generates trading paths (direct, 2-leg, triangular, multi-leg)
- **ArbitrageEngine**: Main engine that detects profitable opportunities above 0.1% threshold

### Network Layer

- **WebSocketClient**: Simulated WebSocket client (placeholder for real Binance WebSocket integration)

### Utilities

- **ConcurrentQueue**: Thread-safe queue for market updates
- **Logger**: Thread-safe logging with INFO, WARN, ERROR levels

## Concurrency Model

The system uses C++17 concurrency primitives:

- **std::thread**: Separate threads for WebSocket client and arbitrage engine
- **std::mutex**: Thread-safe order book updates
- **std::lock_guard**: RAII-based mutex management
- **ConcurrentQueue**: Thread-safe communication between components

### Thread Architecture

1. **WebSocket Thread**: Receives market data and pushes to queue
2. **Engine Thread**: Processes updates and detects arbitrage opportunities

## Arbitrage Route Detection

The system detects arbitrage opportunities through multiple path types:

### 1. Direct Arbitrage
- Compare ARB/USDT direct price with implied USDT prices from other ARB pairs

### 2. Two-Leg Paths
- Example: ARB/EUR → EUR/USDT

### 3. Triangular Arbitrage
- Example: ARB/EUR → ARB/BTC → BTC/USDT

### 4. Multi-Leg Paths
- Complex paths using intermediate currencies (BTC, ETH) as bridges

## Implied USDT Price Calculation

For each ARB pair (ARB/X), the system calculates implied USDT price:

1. **Direct Path**: If X/USDT exists, use ARB/X * X/USDT
2. **BTC Bridge**: If X/BTC and BTC/USDT exist, use ARB/X * X/BTC * BTC/USDT
3. **ETH Bridge**: If X/ETH and ETH/USDT exist, use ARB/X * X/ETH * ETH/USDT

### Supported ARB Pairs

- ARB/USDT (direct)
- ARB/FDUSD
- ARB/USDC
- ARB/TUSD
- ARB/BTC
- ARB/ETH
- ARB/TRY
- ARB/EUR

## Trading Paths

The system assumes the following trading paths:

### Path Examples

1. **ARB/EUR → ARB/USDT**
   - Buy ARB with EUR, sell ARB for USDT
   - Requires: ARB/EUR, EUR/USDT (or EUR/BTC + BTC/USDT)

2. **ARB/BTC → BTC/USDT**
   - Buy ARB with BTC, sell BTC for USDT
   - Requires: ARB/BTC, BTC/USDT

3. **ARB/EUR → ARB/BTC → BTC/USDT**
   - Buy ARB with EUR, sell ARB for BTC, sell BTC for USDT
   - Requires: ARB/EUR, ARB/BTC, BTC/USDT

## Profit Threshold

Default profit threshold: **0.1%**

Only opportunities with profit percentage >= 0.1% are reported.

## Order Book Depth Analysis (Bonus)

For detected opportunities, the system:

1. Analyzes top N levels (default: 5) of order books
2. Calculates maximum tradable amount based on available liquidity
3. Reports tradable amount in ARB tokens

### Tradable Amount Calculation

- Iterates through order book levels
- Accumulates available quantity at each price level
- Returns minimum available amount across all path steps

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./arb_engine
```

## Output Format

When an arbitrage opportunity is detected:

```
[YYYY-MM-DD HH:MM:SS.mmm] [INFO] ARBITRAGE OPPORTUNITY DETECTED:
  Trade Sequence: Buy ARB/EUR → Sell ARB/BTC → Sell BTC/USDT
  Profit: 0.1500%
  Price Levels: ARB/EUR:1.23456789, ARB/BTC:0.00001234, BTC/USDT:45000.12345678
  Max Tradable Amount: 1234.56789012 ARB
```

## Future Enhancements

- Real Binance WebSocket integration (currently simulated)
- Reconnection logic for WebSocket
- Configuration file support
- Performance metrics and monitoring
- Historical data analysis

## Dependencies

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.12+

## License

This is a case study project for educational purposes.

