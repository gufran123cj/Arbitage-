# Binance ARB Arbitrage Engine

A high-performance C++17 arbitrage detection system for Binance Spot market, specifically designed to monitor ARB (Arbitrum) trading pairs and identify profitable arbitrage opportunities in real-time.

## Overview

This project implements a real-time arbitrage detection engine that:
- Connects to Binance WebSocket API to receive live market data
- Monitors 8 ARB trading pairs (ARB/USDT, ARB/FDUSD, ARB/USDC, ARB/TUSD, ARB/BTC, ARB/ETH, ARB/TRY, ARB/EUR)
- Calculates implied USDT prices across different trading routes
- Detects arbitrage opportunities with configurable profit thresholds
- Supports direct, 2-leg, triangular, and multi-leg arbitrage paths

## Requirements

### Required Software

- **CMake**: Version 3.14 or higher
- **Visual Studio**: 2022 (Community, Professional, or Enterprise)
- **vcpkg**: Latest version (for dependency management)
- **Git**: For cloning vcpkg repository

### Required Libraries

- **Boost**: Version 1.84.0 or higher
  - Components: `boost-beast`, `boost-system`
- **OpenSSL**: Version 3.6.0 or higher

### C++ Standard

- **C++17** or higher

## Installation

### Step 1: Install vcpkg

If you don't have vcpkg installed:

```powershell
git clone https://github.com/microsoft/vcpkg.git D:\vcpkg
cd D:\vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### Step 2: Install Dependencies

Install required libraries using vcpkg:

```powershell
D:\vcpkg\vcpkg.exe install boost-beast boost-system openssl:x64-windows
```

This will install:
- Boost.Beast (WebSocket client library)
- Boost.System (system error handling)
- OpenSSL (SSL/TLS support)

**Note**: Installation may take 10-15 minutes on first run.

### Step 3: Clone and Build Project

```powershell
# Navigate to project directory
cd D:\IDEA\Case_Study\C++

# Create build directory
mkdir build
cd build

# Configure CMake with vcpkg toolchain
cmake .. -DCMAKE_TOOLCHAIN_FILE=D:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build the project
cmake --build . --config Debug
```

### Step 4: Run the Application

```powershell
cd Debug
.\arb_engine.exe
```

## Project Structure

```
C++/
├── CMakeLists.txt              # CMake build configuration
├── main.cpp                    # Application entry point
├── README.md                   # This file
├── case.md                     # Project requirements document
└── src/
    └── net/
        ├── WebSocketClient.hpp # WebSocket client header
        └── WebSocketClient.cpp # WebSocket client implementation
```

## Architecture

### WebSocketClient

The `WebSocketClient` class handles:
- Secure WebSocket connections to Binance API
- SSL/TLS handshake and certificate validation
- Real-time market data reception (bookTicker stream)
- Thread-safe data handling
- Automatic reconnection logic

**Current Implementation:**
- Connects to Binance WebSocket API (`wss://stream.binance.com:443`)
- Subscribes to `bookTicker` stream for a given symbol
- Receives real-time bid/ask price updates
- Parses JSON messages and extracts market data

### Data Format

The system receives `bookTicker` messages in the following format:

```json
{
  "u": 6269001103,           // Order book update ID
  "s": "ARBUSDT",            // Symbol
  "b": "0.19030000",         // Best bid price
  "B": "82823.10000000",     // Best bid quantity
  "a": "0.19040000",         // Best ask price
  "A": "27169.70000000"      // Best ask quantity
}
```

## Usage

### Basic Usage

```cpp
#include "src/net/WebSocketClient.hpp"

int main() {
    // Create WebSocket client for ARBUSDT bookTicker stream
    WebSocketClient ws("arbusdt@bookTicker");
    
    // Start receiving data
    ws.start();
    
    // Keep running...
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Stop and cleanup
    ws.stop();
    
    return 0;
}
```

### Supported Symbols

The WebSocket client can connect to any Binance `bookTicker` stream:

- `arbusdt@bookTicker` - ARB/USDT
- `arbbtc@bookTicker` - ARB/BTC
- `arbeth@bookTicker` - ARB/ETH
- `arbusdc@bookTicker` - ARB/USDC
- `arbfdusd@bookTicker` - ARB/FDUSD
- `arbtusd@bookTicker` - ARB/TUSD
- `arbtry@bookTicker` - ARB/TRY
- `arbeur@bookTicker` - ARB/EUR

## Build Configuration

### Debug Build

```powershell
cmake --build . --config Debug
```

### Release Build

```powershell
cmake --build . --config Release
```

### CMake Options

- `CMAKE_TOOLCHAIN_FILE`: Path to vcpkg toolchain file (required)
- `CMAKE_BUILD_TYPE`: Build type (Debug/Release)

## Dependencies

### Boost Libraries

- **Boost.Beast**: Header-only WebSocket and HTTP library
- **Boost.System**: System error codes and error handling
- **Boost.Asio**: Asynchronous I/O operations

### OpenSSL

- **OpenSSL**: SSL/TLS cryptographic library for secure connections

## Troubleshooting

### CMake Cannot Find Boost

**Error**: `Boost gerekli! Lutfen yukleyin.`

**Solution**: 
1. Ensure vcpkg is installed and dependencies are installed
2. Run CMake with vcpkg toolchain:
   ```powershell
   cmake .. -DCMAKE_TOOLCHAIN_FILE=D:\vcpkg\scripts\buildsystems\vcpkg.cmake
   ```

### OpenSSL Not Found

**Error**: `OpenSSL gerekli! Lutfen yukleyin.`

**Solution**:
```powershell
D:\vcpkg\vcpkg.exe install openssl:x64-windows
```

### WebSocket Connection Failed

**Possible Causes**:
- Network connectivity issues
- Binance API endpoint changes
- Firewall blocking WebSocket connections

**Solution**: Check network connection and firewall settings.

### Compilation Errors

**Error**: `'boost/beast/websocket.hpp' file not found`

**Solution**: Ensure Boost.Beast is installed via vcpkg:
```powershell
D:\vcpkg\vcpkg.exe install boost-beast:x64-windows
```

## Development

### Code Standards

- **Naming Conventions**:
  - Classes: PascalCase
  - Methods: camelCase
  - Variables: snake_case
  - Private members: trailing underscore
  - Constants: ALL_CAPS

- **C++17 Features**:
  - Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
  - RAII for resource management
  - `std::thread` for concurrency
  - `std::mutex` for thread synchronization
  - No raw `new`/`delete`
  - No global variables
  - No magic numbers

### Adding New Features

1. Create new source files in appropriate directories
2. Update `CMakeLists.txt` if needed (automatic with `file(GLOB)`)
3. Follow existing code style and conventions
4. Test with Debug build first

## Future Enhancements

- [ ] OrderBook implementation for maintaining bid/ask levels
- [ ] PriceCalculator for implied USDT price calculation
- [ ] Router for generating arbitrage paths
- [ ] ArbitrageEngine for opportunity detection
- [ ] ConcurrentQueue for thread-safe data exchange
- [ ] Logger for structured logging
- [ ] Support for all 8 ARB pairs simultaneously
- [ ] Multi-leg arbitrage detection
- [ ] Profit threshold configuration
- [ ] Order book depth analysis

## License

This project is developed for educational and research purposes.

## References

- [Binance WebSocket API Documentation](https://developers.binance.com/docs/binance-spot-api-docs/web-socket-streams)
- [Boost.Beast Documentation](https://www.boost.org/doc/libs/1_84_0/libs/beast/doc/html/index.html)
- [CMake Documentation](https://cmake.org/documentation/)
- [vcpkg Documentation](https://vcpkg.io/)

## Contact

For questions or issues, please refer to the project documentation or create an issue in the repository.
