#!/bin/bash

set -e
script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$script_dir"
echo "Cleaning up venv..."
rm -rf ./LudorkEnv
echo "Cleaning up SFML..."
find . -type d -name SFML -exec rm -rf {} +
echo "Cleaning up pysf..."
find . -type d -name pysf -exec rm -rf {} +
echo "Cleaning up build..."
find . -type d -name build -exec rm -rf {} +
echo "Cleaning up .pyi, .pyd, and .so files..."
find . -type f \( -name "*.pyi" -o -name "*.pyd" -o -name "*.so" \) -exec rm -f {} +
echo "Cleanup completed"
