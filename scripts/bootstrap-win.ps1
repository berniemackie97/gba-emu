# scripts/bootstrap-win.ps1
$ErrorActionPreference='Stop'

# Resolve repo root whether executed as a script or pasted interactively
if ($PSCommandPath) {
  $Repo = Split-Path -Parent $PSCommandPath
  $Repo = Split-Path -Parent $Repo
} elseif ($PSScriptRoot) {
  $Repo = Split-Path -Parent $PSScriptRoot
} else {
  # Interactive paste: assume current directory is repo root
  $Repo = (Get-Location).Path
}
$Repo = $Repo -replace '\\','/'

# --- VS/MSVC ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $vswhere)) { throw "vswhere not found. Install VS 2022 (Community/Build Tools) with C++ workload." }
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "No MSVC toolset found. Install the VS 2022 C++ workload." }
$msvcRoot = Get-ChildItem -Directory "$vs\VC\Tools\MSVC" | Sort-Object Name -Desc | Select-Object -First 1
$CL   = Join-Path $msvcRoot.FullName "bin\Hostx64\x64\cl.exe"
$LINK = Join-Path $msvcRoot.FullName "bin\Hostx64\x64\link.exe"
$VC_INC     = Join-Path $msvcRoot.FullName "include"
$VC_LIB_X64 = Join-Path $msvcRoot.FullName "lib\x64"
if (!(Test-Path $CL) -or !(Test-Path $LINK)) { throw "cl.exe/link.exe not found under $($msvcRoot.FullName)" }

# --- Windows SDK (rc/mt + include/lib) ---
$kitsBin = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$rc = Get-ChildItem -Recurse -Path $kitsBin -Filter rc.exe -ErrorAction SilentlyContinue | Where-Object { $_.FullName -like "*\x64\rc.exe" } | Sort-Object FullName -Desc | Select-Object -First 1
$mt = Get-ChildItem -Recurse -Path $kitsBin -Filter mt.exe -ErrorAction SilentlyContinue | Where-Object { $_.FullName -like "*\x64\mt.exe" } | Sort-Object FullName -Desc | Select-Object -First 1
if (-not $rc -or -not $mt) {
  winget install -e --id Microsoft.WindowsSDK | Out-Null
  $rc = Get-ChildItem -Recurse -Path $kitsBin -Filter rc.exe | Where-Object { $_.FullName -like "*\x64\rc.exe" } | Sort-Object FullName -Desc | Select-Object -First 1
  $mt = Get-ChildItem -Recurse -Path $kitsBin -Filter mt.exe | Where-Object { $_.FullName -like "*\x64\mt.exe" } | Sort-Object FullName -Desc | Select-Object -First 1
}
if (-not $rc -or -not $mt) { throw "Windows SDK tools not found. Check Windows Kits\10\bin for rc.exe/mt.exe." }

$SdkBinX64 = Split-Path $rc.FullName -Parent
$SdkVer    = Split-Path (Split-Path $SdkBinX64 -Parent) -Leaf
$KitsRoot  = "${env:ProgramFiles(x86)}\Windows Kits\10"
$SDK_INC_SHARED = Join-Path $KitsRoot "Include\$SdkVer\shared"
$SDK_INC_UCRT   = Join-Path $KitsRoot "Include\$SdkVer\ucrt"
$SDK_INC_UM     = Join-Path $KitsRoot "Include\$SdkVer\um"
$SDK_INC_WINRT  = Join-Path $KitsRoot "Include\$SdkVer\winrt"
$SDK_LIB_UCRT   = Join-Path $KitsRoot "Lib\$SdkVer\ucrt\x64"
$SDK_LIB_UM     = Join-Path $KitsRoot "Lib\$SdkVer\um\x64"

# --- Ninja ---
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue)?.Source
if (-not $ninja) { winget install -e --id Ninja-build.Ninja --accept-source-agreements --accept-package-agreements | Out-Null; $ninja = (Get-Command ninja).Source }

# --- vcpkg ---
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = Join-Path $HOME 'vcpkg' }
if (!(Test-Path (Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'))) {
  if (!(Test-Path $env:VCPKG_ROOT)) { git clone https://github.com/microsoft/vcpkg $env:VCPKG_ROOT }
  & "$env:VCPKG_ROOT\bootstrap-vcpkg.bat"
}
$TOOLCHAIN = Join-Path $env:VCPKG_ROOT 'scripts\buildsystems\vcpkg.cmake'

# --- Compose MSVC/SDK env for Ninja ---
$ENV_INCLUDE = @($VC_INC,$SDK_INC_UCRT,$SDK_INC_UM,$SDK_INC_SHARED,$SDK_INC_WINRT) -join ';'
$ENV_LIB     = @($VC_LIB_X64,$SDK_LIB_UCRT,$SDK_LIB_UM) -join ';'
$ENV_PATH    = (@(Split-Path $CL; Split-Path $LINK; $SdkBinX64; Split-Path $ninja) + $env:Path) -join ';'

# --- Write user-local preset with absolute binaryDir ---
$user = @{
  version = 4
  configurePresets = @(@{
    name        = "default"
    displayName = "MSVC+SDK+Ninja+vcpkg (pinned)"
    generator   = "Ninja"
    binaryDir   = "$Repo/build"
    cacheVariables = @{
      "CMAKE_C_COMPILER"     = ($CL -replace '\\','/')
      "CMAKE_CXX_COMPILER"   = ($CL -replace '\\','/')
      "CMAKE_MAKE_PROGRAM"   = ($ninja -replace '\\','/')
      "CMAKE_RC_COMPILER"    = (($rc.FullName) -replace '\\','/')
      "CMAKE_MT"             = (($mt.FullName) -replace '\\','/')
      "CMAKE_TOOLCHAIN_FILE" = ($TOOLCHAIN -replace '\\','/')
    }
    environment = @{
      "INCLUDE" = $ENV_INCLUDE
      "LIB"     = $ENV_LIB
      "Path"    = $ENV_PATH
    }
  })
  buildPresets = @(@{ name="default"; configurePreset="default" })
  testPresets  = @(@{ name="default"; configurePreset="default" })
}
$dest = Join-Path $Repo 'CMakeUserPresets.json'
$user | ConvertTo-Json -Depth 7 | Set-Content $dest -Encoding UTF8
Write-Host ("Wrote user preset to " + $dest) -ForegroundColor Cyan
