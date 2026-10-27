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

// Sanity checks for the in-memory SPIFFS mock itself.
#include <gtest/gtest.h>

#include <FS.h>

class MockFSTest : public ::testing::Test {
 protected:
  void SetUp() override { SPIFFS.reset(); }
};

TEST_F(MockFSTest, WriteTruncatesAppendAppends) {
  File f = SPIFFS.open("/f", "w");
  ASSERT_TRUE(f);
  f.println("one");
  f.close();
  f = SPIFFS.open("/f", "a");
  f.print("two");
  f.close();
  EXPECT_EQ(SPIFFS.readRaw("/f"), "one\r\ntwo");
  SPIFFS.open("/f", "w").close();
  EXPECT_EQ(SPIFFS.readRaw("/f"), "");
}

TEST_F(MockFSTest, ReadLineByLine) {
  SPIFFS.writeRaw("/f", "a\nbb\n");
  File f = SPIFFS.open("/f", "r");
  EXPECT_EQ(f.readStringUntil('\n'), "a");
  EXPECT_TRUE(f.available());
  EXPECT_EQ(f.readStringUntil('\n'), "bb");
  EXPECT_FALSE(f.available());
}

TEST_F(MockFSTest, OpenMissingForReadFails) {
  EXPECT_FALSE(SPIFFS.open("/missing", "r"));
  EXPECT_FALSE(SPIFFS.exists("/missing"));
}

TEST_F(MockFSTest, FailOpenInjection) {
  SPIFFS.failOpen("w");
  EXPECT_FALSE(SPIFFS.open("/f", "w"));
  SPIFFS.failOpen("w", false);
  EXPECT_TRUE(SPIFFS.open("/f", "w"));
}

TEST_F(MockFSTest, DirectoryIteration) {
  SPIFFS.writeRaw("/d/a", "");
  SPIFFS.writeRaw("/d/b", "");
  SPIFFS.writeRaw("/e", "");
  Dir dir = SPIFFS.openDir("/d");
  ASSERT_TRUE(dir.next());
  EXPECT_EQ(dir.fileName(), "/d/a");
  ASSERT_TRUE(dir.next());
  EXPECT_EQ(dir.fileName(), "/d/b");
  EXPECT_FALSE(dir.next());

  File root = SPIFFS.open("/");
  ASSERT_TRUE(root.isDirectory());
  int n = 0;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) n++;
  EXPECT_EQ(n, 3);
}

TEST_F(MockFSTest, RenameFollowsSpiffsRules) {
  SPIFFS.writeRaw("/a", "A");
  SPIFFS.writeRaw("/b", "B");
  EXPECT_FALSE(SPIFFS.rename("/a", "/b"));   // destination exists
  EXPECT_FALSE(SPIFFS.rename("/x", "/y"));   // source missing
  EXPECT_TRUE(SPIFFS.rename("/a", "/c"));
  EXPECT_FALSE(SPIFFS.exists("/a"));
  EXPECT_EQ(SPIFFS.readRaw("/c"), "A");
}

TEST_F(MockFSTest, PowerLossAfterTruncatingOpenDropsLaterWrites) {
  SPIFFS.writeRaw("/f", "old");
  SPIFFS.powerLossAfterOpen("w");
  File f = SPIFFS.open("/f", "w");
  f.print("new");
  f.close();
  EXPECT_FALSE(SPIFFS.remove("/f"));
  SPIFFS.powerOn();
  EXPECT_TRUE(SPIFFS.exists("/f"));
  EXPECT_EQ(SPIFFS.readRaw("/f"), "");   // truncated, new data never landed
}

TEST_F(MockFSTest, PathsLongerThan31CharactersCannotBeCreated) {
  std::string ok(31, 'a'), tooLong(32, 'a');
  ok[0] = tooLong[0] = '/';
  EXPECT_TRUE(SPIFFS.open(ok.c_str(), "w"));
  EXPECT_FALSE(SPIFFS.open(tooLong.c_str(), "w"));
  EXPECT_FALSE(SPIFFS.open(tooLong.c_str(), "a"));
  EXPECT_FALSE(SPIFFS.rename(ok.c_str(), tooLong.c_str()));
}

TEST_F(MockFSTest, PowerLossDuringRemoveLeavesFile) {
  SPIFFS.writeRaw("/f", "abcdef");
  SPIFFS.powerLossOnRemove(true);
  EXPECT_FALSE(SPIFFS.remove("/f"));
  SPIFFS.powerOn();
  EXPECT_EQ(SPIFFS.readRaw("/f"), "abc");
  SPIFFS.powerLossOnRemove(false);
  EXPECT_FALSE(SPIFFS.remove("/f"));
  SPIFFS.powerOn();
  EXPECT_EQ(SPIFFS.readRaw("/f"), "abc");
}
