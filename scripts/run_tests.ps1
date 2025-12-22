# PowerShell script to build and run all tests with comprehensive reporting
# Usage: .\scripts\run_tests.ps1 [-BuildType Debug|Release] [-Verbose] [-Coverage]

param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",

    [switch]$Verbose,
    [switch]$Coverage,
    [switch]$Sanitizers,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Colors for output
function Write-Success { param($Message) Write-Host "✓ $Message" -ForegroundColor Green }
function Write-Info { param($Message) Write-Host "ℹ $Message" -ForegroundColor Cyan }
function Write-Warning { param($Message) Write-Host "⚠ $Message" -ForegroundColor Yellow }
function Write-Error { param($Message) Write-Host "✗ $Message" -ForegroundColor Red }

# Configuration
$ProjectRoot = $PSScriptRoot | Split-Path -Parent
$BuildDir = Join-Path $ProjectRoot "build"

Write-Info "GBA Emulator Test Runner"
Write-Info "========================="
Write-Info "Build Type: $BuildType"
Write-Info "Project Root: $ProjectRoot"
Write-Info ""

# Step 1: Clean build directory if requested
if ($Clean) {
    Write-Info "Cleaning build directory..."
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
        Write-Success "Build directory cleaned"
    }
}

# Step 2: Configure CMake
Write-Info "Configuring CMake..."
Push-Location $ProjectRoot
try {
    $ConfigureArgs = @("--preset", "default")

    if ($Coverage) {
        Write-Warning "Coverage mode is only supported on Linux with GCC"
    }

    cmake @ConfigureArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed"
        exit 1
    }
    Write-Success "CMake configuration completed"
} finally {
    Pop-Location
}

# Step 3: Build
Write-Info "Building project..."
$BuildArgs = @("--build", "build", "--config", $BuildType, "-j")

cmake @BuildArgs
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}
Write-Success "Build completed"

# Step 4: Run Tests
Write-Info "Running tests..."
Push-Location $BuildDir
try {
    $TestArgs = @("-C", $BuildType, "--output-on-failure")

    if ($Verbose) {
        $TestArgs += "--verbose"
    }

    ctest @TestArgs
    $TestExitCode = $LASTEXITCODE

    if ($TestExitCode -eq 0) {
        Write-Success "All tests passed!"
    } else {
        Write-Error "Some tests failed (exit code: $TestExitCode)"
    }
} finally {
    Pop-Location
}

# Step 5: Test Summary
Write-Info ""
Write-Info "Test Summary"
Write-Info "============"

# Parse test results
$TestLogPath = Join-Path $BuildDir "Testing\Temporary\LastTest.log"
if (Test-Path $TestLogPath) {
    $TestLog = Get-Content $TestLogPath -Raw

    # Extract test counts (this is a simple parse, may need adjustment)
    if ($TestLog -match "(\d+) tests? from") {
        $TotalTests = $matches[1]
        Write-Info "Total test executables: $TotalTests"
    }

    # Check for failures
    $FailedTests = Select-String -Path $TestLogPath -Pattern "Failed" -AllMatches
    if ($FailedTests.Count -gt 0) {
        Write-Warning "Found $($FailedTests.Count) failure(s)"
    }
}

# Step 6: List available test executables
Write-Info ""
Write-Info "Available Test Executables:"
Write-Info "==========================="

$TestExecutables = Get-ChildItem -Path $BuildDir -Recurse -Filter "*_test.exe" -File
foreach ($TestExe in $TestExecutables) {
    $TestName = $TestExe.BaseName -replace "_test$", ""
    Write-Host "  • $TestName" -ForegroundColor Cyan

    if ($Verbose) {
        Write-Host "    Path: $($TestExe.FullName)" -ForegroundColor DarkGray
    }
}

Write-Info ""
Write-Info "Tip: Run individual tests with:"
Write-Host "  .\build\$BuildType\<test_name>_test.exe" -ForegroundColor Yellow
Write-Info "Tip: Filter specific test cases with:"
Write-Host "  .\build\$BuildType\<test_name>_test.exe --gtest_filter=TestSuite.TestCase" -ForegroundColor Yellow

exit $TestExitCode
