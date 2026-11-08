#!/usr/bin/env bash
set -euo pipefail

build_type=Debug
verbose=0
coverage=0
clean=0
sanitizer=

usage() {
    printf 'Usage: %s [--build-type Debug|Release] [--verbose] [--coverage] [--clean] [--sanitizer address|undefined]\n' "$0" >&2
}

while (($#)); do
    case "$1" in
        --build-type)
            (($# >= 2)) || { usage; exit 2; }
            build_type="$2"; shift 2 ;;
        --verbose) verbose=1; shift ;;
        --coverage) coverage=1; shift ;;
        --clean) clean=1; shift ;;
        --sanitizer)
            (($# >= 2)) || { usage; exit 2; }
            sanitizer="$2"; shift 2 ;;
        *) usage; exit 2 ;;
    esac
done

case "$build_type" in Debug|Release) ;; *) usage; exit 2;; esac
case "$sanitizer" in ""|address|undefined) ;; *) usage; exit 2;; esac
if ((coverage)) && [[ -n "$sanitizer" ]]; then
    echo 'Coverage and sanitizer instrumentation must be run separately.' >&2
    exit 2
fi
if ((coverage)) && ! command -v lcov >/dev/null; then
    echo 'Coverage requires lcov.' >&2
    exit 2
fi

cd "$(dirname "$0")/.."
if ((clean)); then
    rm -rf build/default
fi

configure_args=(--preset default "-DCMAKE_BUILD_TYPE=$build_type")
if ((coverage)); then
    configure_args+=("-DCMAKE_CXX_FLAGS=--coverage" "-DCMAKE_EXE_LINKER_FLAGS=--coverage")
elif [[ -n "$sanitizer" ]]; then
    configure_args+=("-DCMAKE_CXX_FLAGS=-fsanitize=$sanitizer -fno-omit-frame-pointer"
                    "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=$sanitizer")
fi

cmake "${configure_args[@]}"
cmake --build --preset default --parallel
test_args=(--preset default --output-on-failure)
if ((verbose)); then test_args+=(--verbose); fi
if [[ -n "$sanitizer" ]]; then
    export ASAN_OPTIONS=detect_leaks=1:symbolize=1
    export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
fi
ctest "${test_args[@]}"

if ((coverage)); then
    lcov --capture --directory build/default/CMakeFiles/gba_core.dir --output-file coverage.raw.info
    lcov --remove coverage.raw.info '/usr/*' '*/build/*' '*/tests/*' --output-file coverage.info
    lcov --list coverage.info
fi
