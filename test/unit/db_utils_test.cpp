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

// Tests for DBUtils validation helpers.
#include <gtest/gtest.h>

#include <utils/db_utils.h>

TEST(DBUtils, ValidateListNameAcceptsAllowedCharacters) {
  DBUtils u;
  EXPECT_TRUE(u.validateListName("abc"));
  EXPECT_TRUE(u.validateListName("ABC123"));
  EXPECT_TRUE(u.validateListName("snake_case"));
  EXPECT_TRUE(u.validateListName("kebab-case"));
  EXPECT_TRUE(u.validateListName("9"));
  EXPECT_TRUE(u.validateListName("abcdefghijkl"));     // 12, the maximum
}

TEST(DBUtils, ValidateListNameRejectsNamesLongerThan12) {
  DBUtils u;
  EXPECT_FALSE(u.validateListName("abcdefghijklm"));   // 13
  EXPECT_FALSE(u.validateListName("a_very_long_list_name"));
}

TEST(DBUtils, ValidateListNameRejectsEmptyAndSpecialCharacters) {
  DBUtils u;
  EXPECT_FALSE(u.validateListName(""));
  EXPECT_FALSE(u.validateListName(" "));
  EXPECT_FALSE(u.validateListName("a b"));
  EXPECT_FALSE(u.validateListName("a/b"));
  EXPECT_FALSE(u.validateListName("a.b"));
  EXPECT_FALSE(u.validateListName("a\"b"));
  EXPECT_FALSE(u.validateListName(".."));
}

TEST(DBUtils, IsTokenDigit) {
  DBUtils u;
  EXPECT_TRUE(u.isTokenDigit("0"));
  EXPECT_TRUE(u.isTokenDigit("123456"));
  EXPECT_FALSE(u.isTokenDigit(""));
  EXPECT_FALSE(u.isTokenDigit("-1"));
  EXPECT_FALSE(u.isTokenDigit("1.5"));
  EXPECT_FALSE(u.isTokenDigit("12a"));
  EXPECT_FALSE(u.isTokenDigit(" 1"));
}
