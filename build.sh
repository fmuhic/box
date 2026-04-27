#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

BUILD_DIR="build"

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

# Run the tests
echo "--- Running Tests ---"
ctest --verbose --output-on-failure

cd ..
echo "--- Build and Test Complete ---"
