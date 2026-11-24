#!/usr/bin/env bash
# Format and lint the C++ sources.
#
#   ./lint.sh           apply clang-tidy fixes and clang-format in place
#   ./lint.sh --check   change nothing, fail on any issue (used by the CI)
#
# clang-tidy needs build/compile_commands.json: run ./build.sh (or
# `cmake -B build`) first.
set -Eeuo pipefail
cd "$(dirname "$0")"

MODE="fix"
case "${1:-}" in
    "") ;;
    --check) MODE="check" ;;
    -h | --help)
        sed -n '2,8p' "$0"
        exit 0
        ;;
    *)
        echo "Unknown argument: $1 (use --check or nothing)" >&2
        exit 2
        ;;
esac

if [[ ! -f build/compile_commands.json ]]; then
    echo "build/compile_commands.json not found: run ./build.sh or 'cmake -B build' first." >&2
    exit 1
fi

mapfile -t ALL_FILES < <(find include src tests main.cpp -type f \( -name "*.cpp" -o -name "*.hpp" \) | sort)
# clang-tidy runs on translation units; headers are checked through them.
mapfile -t TIDY_FILES < <(find src tests main.cpp -type f -name "*.cpp" | sort)

status=0

echo "=== clang-format (${#ALL_FILES[@]} files) ==="
if [[ "$MODE" == "check" ]]; then
    clang-format --dry-run --Werror "${ALL_FILES[@]}" || status=1
else
    clang-format -i "${ALL_FILES[@]}"
fi

echo "=== clang-tidy (${#TIDY_FILES[@]} files) ==="
TIDY_ARGS=(--quiet -p build)
if [[ "$MODE" == "check" ]]; then
    TIDY_ARGS+=(--warnings-as-errors='*')
else
    TIDY_ARGS+=(--fix)
fi
for file in "${TIDY_FILES[@]}"; do
    # Drop the "N warnings generated." noise (system headers are filtered out).
    output=$(clang-tidy "${TIDY_ARGS[@]}" "$file" 2>&1) || status=1
    grep -v "warnings generated\.$" <<<"$output" || true
done

# clang-tidy fixes may need re-formatting.
if [[ "$MODE" == "fix" ]]; then
    clang-format -i "${ALL_FILES[@]}"
fi

echo ""
if [[ $status -ne 0 ]]; then
    echo "Lint found issues." >&2
    exit $status
fi
echo "Lint passed."
