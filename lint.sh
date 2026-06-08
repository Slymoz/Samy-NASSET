#!/bin/bash

set -e

echo "=== Clang-tidy ==="

if command -v clang-tidy >/dev/null 2>&1; then
    find . \
        \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.hh" \) \
        -not -path "./build/*" \
        -type f | while read -r file
    do
        clang-tidy \
            -quiet \
            -p . \
            -fix \
            "$file" || true
    done
else
    echo "Clang-tidy not installed."
fi

echo "=== Clang-format ==="

find . \
    \( -name "*.cpp" -o -name "*.hpp" -o -name "*.cc" -o -name "*.hh" \) \
    -not -path "./build/*" \
    -type f \
    -exec clang-format -i {} \;

echo ""
echo "Lint completed successfully."
