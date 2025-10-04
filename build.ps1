# Network Stack Build Script for Windows
# PowerShell version of the build system

param(
    [Parameter(Position=0)]
    [ValidateSet("configure", "build", "debug", "release", "test", "clean", "help")]
    [string]$Command = "help",
    
    [string]$Os = "windows",
    [string]$Arch = "x64"
)

# Configuration
$VcpkgRoot = ".vcpkg"

# Colors for output
function Write-Info {
    param($Message)
    Write-Host "[INFO] $Message" -ForegroundColor Blue
}

function Write-Success {
    param($Message)
    Write-Host "[SUCCESS] $Message" -ForegroundColor Green
}

function Write-Warning {
    param($Message)
    Write-Host "[WARNING] $Message" -ForegroundColor Yellow
}

function Write-Error {
    param($Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

# Show usage information
function Show-Help {
    Write-Host "Network Stack Build Script for Windows"
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [command] [-Os <os>] [-Arch <arch>]"
    Write-Host ""
    Write-Host "Commands:"
    Write-Host "  configure                 Configure CMake build"
    Write-Host "  build, debug              Build debug version"
    Write-Host "  release                   Build release version"
    Write-Host "  test                      Run unit tests"
    Write-Host "  clean                     Clean build directories"
    Write-Host "  help                      Show this help message"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Os <os>                  Target OS (windows)"
    Write-Host "  -Arch <arch>              Target architecture (x64)"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1 release       # Build release version"
    Write-Host "  .\build.ps1 test          # Run unit tests"
    Write-Host ""
    Write-Host "Configuration Examples:"
    Write-Host "  # Run with default config"
    Write-Host "  .\build\stack.exe"
    Write-Host ""
    Write-Host "  # Run with custom config file"
    Write-Host "  .\build\stack.exe --config examples\production-proxy.toml"
    Write-Host ""
    Write-Host "  # Run with command-line options"
    Write-Host "  .\build\stack.exe --socks5-host proxy.example.com --log-level debug"
}

# Setup vcpkg
function Setup-Vcpkg {
    if (-not (Test-Path $VcpkgRoot)) {
        Write-Info "vcpkg not found. Setting up..."
        git clone --depth=1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
        & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
    }
}

# Configure CMake
function Configure-Build {
    param($BuildType, $BuildDir)
    
    $VcpkgTriplet = "$Arch-windows"
    $ToolchainFile = "$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    
    Write-Info "Configuring for $Os-$Arch ($BuildType)..."
    
    cmake -B $BuildDir -S . `
        -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile" `
        -DVCPKG_TARGET_TRIPLET="$VcpkgTriplet" `
        -DCMAKE_BUILD_TYPE="$BuildType" `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=1
    
    # Copy compile commands for IDE support
    if (Test-Path "$BuildDir\compile_commands.json") {
        Copy-Item "$BuildDir\compile_commands.json" "compile_commands.json" -Force
    }
}

# Clean build directories
function Clean-Build {
    Write-Info "Cleaning build directories..."
    Get-ChildItem -Path "." -Directory -Name "cmake-build-*" | ForEach-Object {
        Remove-Item $_ -Recurse -Force
    }
    if (Test-Path "compile_commands.json") {
        Remove-Item "compile_commands.json" -Force
    }
    Write-Success "Build directories cleaned"
}

# Run tests
function Run-Tests {
    Write-Info "Running unit tests..."
    
    $DebugDir = "cmake-build-$Os-$Arch-debug"
    $ReleaseDir = "cmake-build-$Os-$Arch-release"
    
    if (Test-Path "$DebugDir\CMakeCache.txt") {
        Push-Location $DebugDir
        ctest --output-on-failure
        Pop-Location
    } elseif (Test-Path "$ReleaseDir\CMakeCache.txt") {
        Push-Location $ReleaseDir
        ctest --output-on-failure
        Pop-Location
    } else {
        Write-Warning "No build found. Building debug version first..."
        Configure-Build "Debug" $DebugDir
        cmake --build $DebugDir
        Push-Location $DebugDir
        ctest --output-on-failure
        Pop-Location
    }
    
    Write-Success "Unit tests completed"
}

# Main script logic
try {
    $DebugDir = "cmake-build-$Os-$Arch-debug"
    $ReleaseDir = "cmake-build-$Os-$Arch-release"
    
    Setup-Vcpkg
    
    switch ($Command) {
        "configure" {
            Configure-Build "Debug" $DebugDir
            Write-Success "Configuration complete."
        }
        
        { $_ -in @("build", "debug") } {
            if (-not (Test-Path "$DebugDir\CMakeCache.txt")) {
                Configure-Build "Debug" $DebugDir
            }
            Write-Info "Building (Debug)..."
            cmake --build $DebugDir
            Write-Success "Debug build complete."
            Write-Info "Run with: .\$DebugDir\stack.exe --help"
        }
        
        "release" {
            if (-not (Test-Path "$ReleaseDir\CMakeCache.txt")) {
                Configure-Build "Release" $ReleaseDir
            }
            Write-Info "Building (Release)..."
            cmake --build $ReleaseDir
            Write-Success "Release build complete."
            Write-Info "Run with: .\$ReleaseDir\stack.exe --help"
        }
        
        "test" {
            Run-Tests
        }
        
        "clean" {
            Clean-Build
        }
        
        "help" {
            Show-Help
        }
        
        default {
            Write-Error "Unknown command: $Command"
            Write-Host ""
            Show-Help
            exit 1
        }
    }
} catch {
    Write-Error "Build script failed: $_"
    exit 1
}
