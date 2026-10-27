// The MIT License (MIT)
//
// Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Tests for DBFilterProcessor in isolation (no filesystem involved).
// filterParse() returns true on MISMATCH, false when the document matches.
#include <gtest/gtest.h>

#include <ArduinoJson.h>
#include <filter/filter_processor.h>

namespace {

bool matches(const char* filter, const char* document) {
  JsonDocument f, d;
  EXPECT_FALSE(deserializeJson(f, filter)) << filter;
  EXPECT_FALSE(deserializeJson(d, document)) << document;
  DBFilterProcessor p;
  return !p.filterParse(f.as<JsonObject>(), d.as<JsonVariant>(), false);
}

}  // namespace

TEST(FilterProcessor, EmptyFilterMatchesAnything) {
  EXPECT_TRUE(matches("{}", R"({"a":1})"));
  EXPECT_TRUE(matches("{}", "{}"));
}

TEST(FilterProcessor, ScalarEquality) {
  EXPECT_TRUE(matches(R"({"a":1})", R"({"a":1})"));
  EXPECT_FALSE(matches(R"({"a":1})", R"({"a":2})"));
  EXPECT_TRUE(matches(R"({"a":1.25})", R"({"a":1.25})"));
  EXPECT_TRUE(matches(R"({"a":"x"})", R"({"a":"x"})"));
  EXPECT_FALSE(matches(R"({"a":"x"})", R"({"a":"X"})"));
  EXPECT_TRUE(matches(R"({"a":true})", R"({"a":true})"));
  EXPECT_FALSE(matches(R"({"a":true})", R"({"a":false})"));
}

TEST(FilterProcessor, IntAndFloatCompareNumerically) {
  EXPECT_TRUE(matches(R"({"a":2})", R"({"a":2.0})"));
}

TEST(FilterProcessor, MissingFieldOrTypeMismatch) {
  EXPECT_FALSE(matches(R"({"a":1})", R"({"b":1})"));
  EXPECT_FALSE(matches(R"({"a":1})", R"({"a":"1"})"));
  EXPECT_FALSE(matches(R"({"a":"1"})", R"({"a":1})"));
  EXPECT_FALSE(matches(R"({"a":true})", R"({"a":1})"));
}

TEST(FilterProcessor, NullOrArrayFilterValueNeverMatches) {
  EXPECT_FALSE(matches(R"({"a":null})", R"({"a":null})"));
  EXPECT_FALSE(matches(R"({"a":[1]})", R"({"a":[1]})"));
}

TEST(FilterProcessor, DotPathNavigation) {
  EXPECT_TRUE(matches(R"({"a.b.c":5})", R"({"a":{"b":{"c":5}}})"));
  EXPECT_FALSE(matches(R"({"a.b.c":5})", R"({"a":{"b":{"c":6}}})"));
  EXPECT_TRUE(matches(R"({"a.1":"y"})", R"({"a":["x","y"]})"));
  EXPECT_TRUE(matches(R"({"a.1.k":3})", R"({"a":[{},{"k":3}]})"));
  EXPECT_FALSE(matches(R"({"a.5":"y"})", R"({"a":["x","y"]})"));
  EXPECT_FALSE(matches(R"({"a.x":"y"})", R"({"a":["x","y"]})"));
  EXPECT_FALSE(matches(R"({"a.b":"y"})", R"({"a":"str"})"));
}

TEST(FilterProcessor, RangeOperatorsAreStrictOrInclusive) {
  const char* doc = R"({"n":10})";
  EXPECT_TRUE(matches(R"({"n":{"$gt":9}})", doc));
  EXPECT_FALSE(matches(R"({"n":{"$gt":10}})", doc));
  EXPECT_TRUE(matches(R"({"n":{"$gte":10}})", doc));
  EXPECT_FALSE(matches(R"({"n":{"$gte":11}})", doc));
  EXPECT_TRUE(matches(R"({"n":{"$lt":11}})", doc));
  EXPECT_FALSE(matches(R"({"n":{"$lt":10}})", doc));
  EXPECT_TRUE(matches(R"({"n":{"$lte":10}})", doc));
  EXPECT_FALSE(matches(R"({"n":{"$lte":9}})", doc));
}

TEST(FilterProcessor, RangeOperatorRequiresNumericOperand) {
  EXPECT_FALSE(matches(R"({"n":{"$gt":"5"}})", R"({"n":10})"));
  EXPECT_FALSE(matches(R"({"n":{"$gt":5}})", R"({"n":"10"})"));
}

TEST(FilterProcessor, EqOperator) {
  EXPECT_TRUE(matches(R"({"n":{"$eq":10}})", R"({"n":10})"));
  EXPECT_TRUE(matches(R"({"s":{"$eq":"hi"}})", R"({"s":"hi"})"));
  EXPECT_TRUE(matches(R"({"b":{"$eq":false}})", R"({"b":false})"));
  EXPECT_FALSE(matches(R"({"n":{"$eq":10}})", R"({"n":11})"));
}

TEST(FilterProcessor, EleMatch) {
  EXPECT_TRUE(matches(R"({"arr":{"$eleMatch":{"$eq":53.4}}})", R"({"arr":[1,53.4,2]})"));
  EXPECT_FALSE(matches(R"({"arr":{"$eleMatch":{"$eq":7}}})", R"({"arr":[1,2]})"));
  EXPECT_TRUE(matches(R"({"arr":{"$eleMatch":{"$lte":1}}})", R"({"arr":[5,1]})"));
  EXPECT_TRUE(matches(R"({"arr":{"$eleMatch":{"k":"v"}}})", R"({"arr":[{"k":"x"},{"k":"v"}]})"));
  EXPECT_FALSE(matches(R"({"arr":{"$eleMatch":{"$eq":1}}})", R"({"arr":1})"));
}

TEST(FilterProcessor, MultiKeyFilterMatchesWhenAllKeysMatch) {
  EXPECT_TRUE(matches(R"({"a":1,"b":2})", R"({"a":1,"b":2})"));
  EXPECT_FALSE(matches(R"({"a":1,"b":2})", R"({"a":1,"b":3})"));
  EXPECT_FALSE(matches(R"({"a":1,"b":2})", R"({"a":9,"b":2})"));
  EXPECT_FALSE(matches(R"({"a":1,"b":{"$gt":1}})", R"({"a":9,"b":2})"));
}

TEST(FilterProcessor, AllOperatorsOnAFieldMustMatch) {
  EXPECT_TRUE(matches(R"({"n":{"$gt":1,"$lt":5}})", R"({"n":3})"));
  EXPECT_FALSE(matches(R"({"n":{"$gt":1,"$lt":5}})", R"({"n":0})"));
  EXPECT_FALSE(matches(R"({"n":{"$gt":1,"$lt":5}})", R"({"n":7})"));
  EXPECT_FALSE(matches(R"({"n":{"$lt":5,"$gt":1}})", R"({"n":0})"));
}

TEST(FilterProcessor, EleMatchOnEmptyArrayDoesNotMatch) {
  EXPECT_FALSE(matches(R"({"arr":{"$eleMatch":{"$eq":1}}})", R"({"arr":[]})"));
  EXPECT_FALSE(matches(R"({"arr":{"$eleMatch":{"k":"v"}}})", R"({"arr":[]})"));
}

TEST(FilterProcessor, EleMatchObjectNeedsAllKeysInOneElement) {
  const char* doc = R"({"arr":[{"k":"v","n":1},{"k":"w","n":2}]})";
  EXPECT_TRUE(matches(R"({"arr":{"$eleMatch":{"k":"w","n":2}}})", doc));
  EXPECT_FALSE(matches(R"({"arr":{"$eleMatch":{"k":"v","n":2}}})", doc));
}
