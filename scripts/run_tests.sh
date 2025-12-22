#!/bin/bash
# Bash script to build and run all tests with comprehensive reporting
# Usage: ./scripts/run_tests.sh [--build-type Debug|Release] [--verbose] [--coverage] [--clean]

set -e

# Default values
BUILD_TYPE="Debug"
VERBOSE=""
COVERAGE=0
CLEAN=0
SANITIZER=""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Functions
print_success() { echo -e "${GREEN}✓ $1${NC}"; }
print_info() { echo -e "${CYAN}ℹ $1${NC}"; }
print_warning() { echo -e "${YELLOW}⚠ $1${NC}"; }
print_error() { echo -e "${RED}✗ $1${NC}"; }

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE="--verbose"
            shift
            ;;
        --coverage)
            COVERAGE=1
            shift
            ;;
        --clean)
            CLEAN=1
            shift
            ;;
        --sanitizer)
            SANITIZER="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--build-type Debug|Release] [--verbose] [--coverage] [--clean] [--sanitizer address|undefined]"
            exit 1
            ;;
    esac
done

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

print_info "GBA Emulator Test Runner"
print_info "========================="
print_info "Build Type: $BUILD_TYPE"
print_info "Project Root: $PROJECT_ROOT"
echo ""

# Step 1: Clean build directory if requested
if [ $CLEAN -eq 1 ]; then
    print_info "Cleaning build directory..."
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned"
    fi
fi

# Step 2: Configure CMake
print_info "Configuring CMake..."
cd "$PROJECT_ROOT"

CMAKE_ARGS=(-B build -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE)

if [ $COVERAGE -eq 1 ]; then
    print_info "Enabling coverage instrumentation..."
    CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage")
    CMAKE_ARGS+=(-DCMAKE_C_FLAGS="--coverage -fprofile-arcs -ftest-coverage")
    CMAKE_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS="--coverage")
fi

if [ -n "$SANITIZER" ]; then
    print_info "Enabling $SANITIZER sanitizer..."
    CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-fsanitize=$SANITIZER -fno-omit-frame-pointer")
    CMAKE_ARGS+=(-DCMAKE_C_FLAGS="-fsanitize=$SANITIZER -fno-omit-frame-pointer")
    CMAKE_ARGS+=(-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=$SANITIZER")
fi

cmake "${CMAKE_ARGS[@]}"
print_success "CMake configuration completed"

# Step 3: Build
print_info "Building project..."
cmake --build build -j

if [ $? -ne 0 ]; then
    print_error "Build failed"
    exit 1
fi
print_success "Build completed"

# Step 4: Run Tests
print_info "Running tests..."
cd "$BUILD_DIR"

TEST_ARGS=(--output-on-failure)
if [ -n "$VERBOSE" ]; then
    TEST_ARGS+=(--verbose)
fi

if [ -n "$SANITIZER" ]; then
    export ASAN_OPTIONS=detect_leaks=1:symbolize=1
    export UBSAN_OPTIONS=print_stacktrace=1
fi

ctest "${TEST_ARGS[@]}"
TEST_EXIT_CODE=$?

cd "$PROJECT_ROOT"

if [ $TEST_EXIT_CODE -eq 0 ]; then
    print_success "All tests passed!"
else
    print_error "Some tests failed (exit code: $TEST_EXIT_CODE)"
fi

# Step 5: Generate coverage report if requested
if [ $COVERAGE -eq 1 ]; then
    print_info "Generating coverage report..."

    if command -v lcov &> /dev/null; then
        lcov --capture --directory build --output-file coverage.info
        lcov --remove coverage.info '/usr/*' '*/build/*' '*/tests/*' --output-file coverage.info
        lcov --list coverage.info

        if command -v genhtml &> /dev/null; then
            genhtml coverage.info --output-directory coverage_html
            print_success "Coverage report generated in coverage_html/"
            print_info "Open coverage_html/index.html in a browser to view"
        fi
    else
        print_warning "lcov not found. Install lcov to generate coverage reports."
    fi
fi

# Step 6: Test Summary
echo ""
print_info "Test Summary"
print_info "============"

# Parse test results
TEST_LOG="$BUILD_DIR/Testing/Temporary/LastTest.log"
if [ -f "$TEST_LOG" ]; then
    TOTAL_TESTS=$(grep -c "Test #" "$TEST_LOG" || echo "0")
    print_info "Total test cases run: $TOTAL_TESTS"

    FAILED_TESTS=$(grep -c "Failed" "$TEST_LOG" || echo "0")
    if [ "$FAILED_TESTS" -gt 0 ]; then
        print_warning "Found $FAILED_TESTS failure(s)"
    fi
fi

# Step 7: List available test executables
echo ""
print_info "Available Test Executables:"
print_info "==========================="

find "$BUILD_DIR" -type f -name "*_test" | while read -r test_exe; do
    test_name=$(basename "$test_exe" | sed 's/_test$//')
    echo -e "  ${CYAN}• $test_name${NC}"

    if [ -n "$VERBOSE" ]; then
        echo -e "    ${NC}Path: $test_exe"
    fi
done

echo ""
print_info "Tip: Run individual tests with:"
echo -e "  ${YELLOW}./$BUILD_DIR/<test_name>_test${NC}"
print_info "Tip: Filter specific test cases with:"
echo -e "  ${YELLOW}./$BUILD_DIR/<test_name>_test --gtest_filter=TestSuite.TestCase${NC}"

exit $TEST_EXIT_CODE
