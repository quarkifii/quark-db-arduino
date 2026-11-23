#!/usr/bin/env bash
# Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
# SPDX-License-Identifier: MIT
# See LICENSE at the repository root for the full license text.

# Runs inside the container: configure, build and run the QuarkDB tests.
# Expects the repository mounted at /quarkdb and a build cache at /build.
#
#   --cloud   run only the cloud backup tests (every suite named Cloud*:
#             CloudMqttMock, CloudTransport, CloudTopicMatch, CloudOplog,
#             CloudSync); any other argument is passed to gtest as is
set -euo pipefail

CLOUD_ONLY=0
USER_FILTER=0
GTEST_ARGS=()
for arg in "$@"; do
  case "$arg" in
    --cloud) CLOUD_ONLY=1 ;;
    --gtest_filter=*) USER_FILTER=1; GTEST_ARGS+=("$arg") ;;
    *) GTEST_ARGS+=("$arg") ;;
  esac
done
if [ "$CLOUD_ONLY" = 1 ]; then
  if [ "$USER_FILTER" = 1 ]; then
    echo "--cloud ignored: an explicit --gtest_filter was given"
  else
    GTEST_ARGS+=("--gtest_filter=Cloud*")
    echo "Test config: cloud backup (suites Cloud*)"
  fi
fi

BUILD_DIR="/build/tests"
cmake -S /quarkdb/test -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DARDUINOJSON_SRC_DIR=/opt/ArduinoJson/src \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/opt/googletest \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON >/dev/null
echo "Building tests..."
# Keep output quiet on success; show the full log only when the build fails.
if ! cmake --build "$BUILD_DIR" -j"$(nproc)" >/tmp/build.log 2>&1; then
  cat /tmp/build.log
  echo "Build failed"
  exit 1
fi

# Tests always run under AddressSanitizer, LeakSanitizer and UBSan.
# lsan.supp only covers the library's known one-time helper allocations, so
# any other leak still fails the run.
export ASAN_OPTIONS="detect_leaks=1:detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1"
export LSAN_OPTIONS="suppressions=/etc/quarkdb/lsan.supp:print_suppressions=0"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

status=0
for platform in esp8266 esp32; do
  echo "================ $platform ================"
  "$BUILD_DIR/quarkdb_tests_$platform" --gtest_color=yes "${GTEST_ARGS[@]+"${GTEST_ARGS[@]}"}" || status=1
done
exit $status
