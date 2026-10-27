#!/usr/bin/env bash
# Build and run the QuarkDB host unit tests inside a Docker container
# (no Arduino board, SPIFFS or local toolchain needed).
#
#   ./run_tests.sh              build + run all tests for esp8266 and esp32
#                               (always under AddressSanitizer, LeakSanitizer and UBSan)
#   ./run_tests.sh --gtest_filter='QuarkDBTest.Update*'   any gtest flag works
#   ./run_tests.sh --clean      drop the cached build volume first
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
IMAGE="${QUARKDB_TEST_IMAGE:-quarkdb-tests:latest}"
VOLUME="${QUARKDB_TEST_VOLUME:-quarkdb-test-build}"

ARGS=()
for arg in "$@"; do
  if [ "$arg" = "--clean" ]; then
    docker volume rm -f "$VOLUME" >/dev/null
  else
    ARGS+=("$arg")
  fi
done

# Cached after the first run; rebuilds only if docker/ changes.
docker build -q -t "$IMAGE" "$HERE/docker" >/dev/null

TTY_FLAGS=()
[ -t 1 ] && TTY_FLAGS=(-t)

# Sources are mounted read-only; build output lives in a named volume so
# nothing is written into the repository and rebuilds are incremental.
exec docker run --rm "${TTY_FLAGS[@]+"${TTY_FLAGS[@]}"}" \
  -v "$REPO:/quarkdb:ro" \
  -v "$VOLUME:/build" \
  "$IMAGE" "${ARGS[@]+"${ARGS[@]}"}"
