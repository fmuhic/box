#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

BUILD_DIR="build"

# All first-party sources (the build tree is excluded).
SOURCES=$(find include src tests \( -name '*.c' -o -name '*.h' \))

# --- Enforce formatting (.clang-format) ---
if command -v clang-format >/dev/null 2>&1; then
    echo "--- Checking formatting (clang-format) ---"
    if ! clang-format --dry-run --Werror $SOURCES; then
        echo "!!! Formatting drift detected. Fix with:"
        echo "    find include src tests \\( -name '*.c' -o -name '*.h' \\) | xargs clang-format -i"
        exit 1
    fi
else
    echo "--- clang-format not found, skipping format check ---"
fi

# Create the build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    echo "--- Creating build directory: $BUILD_DIR ---"
    mkdir "$BUILD_DIR"
fi
cd "$BUILD_DIR"

#  Run CMake configuration
echo "--- Configuring project with CMake ---"
cmake ..
echo "--- Compiling ---"
cmake --build .

cd ..

# --- Lint (.clang-tidy) — advisory: reports findings but does not fail the build ---
if command -v clang-tidy >/dev/null 2>&1; then
    echo "--- Linting (clang-tidy) ---"
    clang-tidy -p "$BUILD_DIR" $(find src tests -name '*.c') --quiet || true
else
    echo "--- clang-tidy not found, skipping lint ---"
fi

# Run the tests
echo "--- Running Tests ---"
cd "$BUILD_DIR"
ctest --verbose --output-on-failure
cd ..

echo "--- Build and Test Complete ---"
