#!/bin/bash

set -e

echo "=== Clang-tidy ==="

find . \
    \( -name "*.cpp" -o -name "*.hpp" \) \
    -not -path "./build/*" \
    -type f | while read -r file
do
    clang-tidy \
        -quiet \
        -p . \
        -fix \
        "$file" \
        2>/dev/null || true
done

echo "=== Clang-format ==="

find . \
    \( -name "*.cpp" -o -name "*.hpp" \) \
    -not -path "./build/*" \
    -type f \
    -exec clang-format -i {} \;

echo ""
echo "Lint completed successfully."
