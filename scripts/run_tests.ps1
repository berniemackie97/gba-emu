param(
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Debug",
    [switch]$Verbose,
    [switch]$Coverage,
    [switch]$Sanitizers,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
if ($Coverage -or $Sanitizers) {
    throw "Coverage and sanitizer instrumentation is supported by the Linux CI jobs, not this Windows runner."
}

$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
try {
    if ($Clean -and (Test-Path "build/windows")) {
        Remove-Item -Recurse -Force "build/windows"
    }

    cmake --preset windows
    if ($LASTEXITCODE -ne 0) { throw "Configure failed ($LASTEXITCODE)." }

    cmake --build --preset windows --config $BuildType --parallel
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }

    $testArgs = @("--preset", "windows", "--build-config", $BuildType, "--output-on-failure")
    if ($Verbose) { $testArgs += "--verbose" }
    ctest @testArgs
    if ($LASTEXITCODE -ne 0) { throw "Tests failed ($LASTEXITCODE)." }
} finally {
    Pop-Location
}
