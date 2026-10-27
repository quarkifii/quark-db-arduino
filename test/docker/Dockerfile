# Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
# SPDX-License-Identifier: MIT
# See LICENSE at the repository root for the full license text.

# Self-contained toolchain for the QuarkDB host unit tests.
# GoogleTest and ArduinoJson are baked in so test runs need no network.
FROM ubuntu:22.04

ARG GTEST_VERSION=1.15.2
ARG ARDUINOJSON_VERSION=7.3.0

RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      cmake make g++ ca-certificates curl \
 && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/googletest /opt/ArduinoJson \
 && curl -fsSL https://github.com/google/googletest/archive/refs/tags/v${GTEST_VERSION}.tar.gz \
      | tar -xz --strip-components=1 -C /opt/googletest \
 && curl -fsSL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v${ARDUINOJSON_VERSION}.tar.gz \
      | tar -xz --strip-components=1 -C /opt/ArduinoJson

COPY entrypoint.sh /usr/local/bin/quarkdb-test
COPY lsan.supp /etc/quarkdb/lsan.supp
RUN chmod +x /usr/local/bin/quarkdb-test

WORKDIR /build
ENTRYPOINT ["quarkdb-test"]
