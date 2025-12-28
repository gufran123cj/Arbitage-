# Binance ARB Arbitrage Engine - Automatic Setup Script
# This script automatically installs all dependencies and sets up the project
# Run from project root: .\setup.ps1

param(
    [string]$VcpkgPath = "$env:USERPROFILE\vcpkg"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Binance ARB Arbitrage Engine Setup" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Git is available
try {
    $null = git --version
    Write-Host "Git found" -ForegroundColor Green
} catch {
    Write-Host "Error: Git is not installed or not in PATH!" -ForegroundColor Red
    Write-Host "Please install Git from: https://git-scm.com/download/win" -ForegroundColor Yellow
    exit 1
}

# Check if CMake is available
try {
    $null = cmake --version
    Write-Host "CMake found" -ForegroundColor Green
} catch {
    Write-Host "Error: CMake is not installed or not in PATH!" -ForegroundColor Red
    Write-Host "Please install CMake from: https://cmake.org/download/" -ForegroundColor Yellow
    exit 1
}

# Check if vcpkg exists
if (-not (Test-Path $VcpkgPath)) {
    Write-Host "vcpkg not found at $VcpkgPath" -ForegroundColor Yellow
    Write-Host "Installing vcpkg from GitHub..." -ForegroundColor Yellow
    
    # Create parent directory if it doesn't exist
    $parentDir = Split-Path -Parent $VcpkgPath
    if (-not (Test-Path $parentDir)) {
        New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
    }
    
    # Clone vcpkg
    Write-Host "Cloning vcpkg repository..." -ForegroundColor Yellow
    git clone https://github.com/microsoft/vcpkg.git $VcpkgPath
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error cloning vcpkg repository!" -ForegroundColor Red
        exit 1
    }
    
    # Bootstrap vcpkg
    Write-Host "Bootstrapping vcpkg..." -ForegroundColor Yellow
    Push-Location $VcpkgPath
    .\bootstrap-vcpkg.bat
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error bootstrapping vcpkg!" -ForegroundColor Red
        Pop-Location
        exit 1
    }
    
    Pop-Location
    Write-Host "vcpkg installed successfully!" -ForegroundColor Green
} else {
    Write-Host "vcpkg found at $VcpkgPath" -ForegroundColor Green
}

# Install dependencies
Write-Host ""
Write-Host "Installing dependencies..." -ForegroundColor Yellow
Write-Host "This may take 10-15 minutes on first run..." -ForegroundColor Yellow
Write-Host ""

$vcpkgExe = Join-Path $VcpkgPath "vcpkg.exe"

if (-not (Test-Path $vcpkgExe)) {
    Write-Host "Error: vcpkg.exe not found at $vcpkgExe" -ForegroundColor Red
    exit 1
}

# Install Boost.Beast, Boost.System, OpenSSL, and FTXUI
Write-Host "Installing: boost-beast, boost-system, openssl, ftxui..." -ForegroundColor Cyan
& $vcpkgExe install boost-beast:x64-windows boost-system:x64-windows openssl:x64-windows ftxui:x64-windows

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error installing dependencies!" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Dependencies installed successfully!" -ForegroundColor Green

# Create build directory
Write-Host ""
Write-Host "Creating build directory..." -ForegroundColor Yellow
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow
Push-Location "build"
$cmakeToolchain = Join-Path $VcpkgPath "scripts\buildsystems\vcpkg.cmake"
cmake .. -DCMAKE_TOOLCHAIN_FILE=$cmakeToolchain

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error configuring CMake!" -ForegroundColor Red
    Pop-Location
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "Setup completed successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "To build the project, run:" -ForegroundColor Cyan
Write-Host "  cd build" -ForegroundColor White
Write-Host "  cmake --build . --config Debug" -ForegroundColor White
Write-Host ""
Write-Host "To run the application:" -ForegroundColor Cyan
Write-Host "  .\Debug\arb_engine.exe" -ForegroundColor White
Write-Host ""

Pop-Location
