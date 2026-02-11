#!/bin/bash
# Format sources using clang-format

# Ensure we are in the project root or adjust paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

echo "Formatting C++ sources in $PROJECT_ROOT..."

find "$PROJECT_ROOT/include" "$PROJECT_ROOT/src" "$PROJECT_ROOT/examples" \
    -name "*.hpp" -o -name "*.cpp" -o -name "*.h" -o -name "*.c" | \
    xargs clang-format -i -style=file

echo "Done."
