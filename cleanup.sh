#!/bin/bash

set -e
./del_cache.sh
script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$script_dir"
echo "Cleaning up venv..."
rm -rf ./LudorkEnv
echo "Cleaning up SFML..."
rm -rf ./C_Extensions/SFML
echo "Cleaning up pysf..."
rm -rf ./pysf
echo "Cleaning up build..."
rm -rf ./build
echo "Cleaning up .pyi, .pyd, and .so files..."
find . -type f \( -name "*.pyi" -o -name "*.pyd" -o -name "*.so" \) -exec rm -f {} +
echo "Cleanup completed"
