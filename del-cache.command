#!/bin/bash

target_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Search and delete __pycache__ folders in $target_dir"
find "$target_dir" -type d -name "__pycache__" -print -exec rm -rf {} \; 2>/dev/null || true

echo "All __pycache__ are deleted"
read -p "Press any key to continue..."