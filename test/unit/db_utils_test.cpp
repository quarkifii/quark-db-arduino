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
