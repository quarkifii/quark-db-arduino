// Power-loss safety: update/delete rewrite a list through a temp file that is
// swapped in only once complete, and recoverList() finishes or rolls back an
// interrupted swap on the next access.
#include "test_helpers.h"

class PowerLossTest : public QuarkDBTest {
 protected:
  // Simulates a reboot: power back on and the DB initialised again.
  void reboot() {
    SPIFFS.powerOn();
    ASSERT_TRUE(db.init(QUARKDB_SPIFFS_FILE_TYPE));
  }
  const Names everyone{"alice", "bob", "carol", "dave", "erin"};
};

TEST_F(PowerLossTest, UpdateWritesThroughTempFileAndCleansUp) {
  seedPeople();
  ASSERT_EQ(update("people", R"({"name":"alice"})", R"({"name":"ann"})"), 1);
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));
  EXPECT_EQ(findNames("people", "{}"), (Names{"ann", "bob", "carol", "dave", "erin"}));
}

TEST_F(PowerLossTest, PowerLossWhileWritingTempFileDuringUpdateKeepsOriginal) {
  seedPeople();
  SPIFFS.powerLossAfterOpen("w");
  update("people", R"({"name":"alice"})", R"({"name":"ann"})");
  reboot();
  EXPECT_EQ(db.getRecordCount("people"), 5);
  EXPECT_EQ(findNames("people", "{}"), everyone);
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));  // partial temp discarded
}

TEST_F(PowerLossTest, PowerLossWhileWritingTempFileDuringDeleteKeepsOriginal) {
  seedPeople();
  SPIFFS.powerLossAfterOpen("w");
  db.deleteRecords("people", R"({"name":"alice"})");
  reboot();
  EXPECT_EQ(db.getRecordCount("people"), 5);
  EXPECT_EQ(findNames("people", "{}"), everyone);
}

TEST_F(PowerLossTest, PartialTempFileNextToListIsDiscarded) {
  seedPeople();
  std::string original = SPIFFS.readRaw(listPath("people"));
  SPIFFS.writeRaw(tmpPath("people"), "{\"name\":\"half");
  reboot();
  EXPECT_EQ(findNames("people", "{}"), everyone);
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")), original);
}

TEST_F(PowerLossTest, CompleteTempFileIsSwappedInWhenListFileIsGone) {
  // State after power loss between removing the list file and the rename.
  SPIFFS.writeRaw(tmpPath("people"), "{\"name\":\"ann\"}\r\n{\"name\":\"bob\"}\r\n");
  reboot();
  EXPECT_EQ(db.getRecordCount("people"), 2);
  EXPECT_TRUE(SPIFFS.exists(listPath("people").c_str()));
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));
  EXPECT_EQ(findNames("people", "{}"), (Names{"ann", "bob"}));
}

TEST_F(PowerLossTest, EveryOperationRecoversAnInterruptedSwap) {
  const std::string content = "{\"name\":\"ann\"}\r\n";
  auto interrupted = [&] {
    SPIFFS.reset();
    SPIFFS.writeRaw(tmpPath("people"), content);
  };

  interrupted();
  JsonDocument result;
  EXPECT_EQ(db.getRecords("people", "{}", &result), 1);

  interrupted();
  EXPECT_TRUE(add("people", R"({"name":"bob"})"));
  EXPECT_EQ(db.getRecordCount("people"), 2);

  interrupted();
  EXPECT_EQ(update("people", "{}", R"({"name":"x"})"), 1);

  interrupted();
  EXPECT_EQ(db.deleteRecords("people", "{}"), 1);

  interrupted();
  EXPECT_FALSE(db.createList("people"));  // recovered, so it already exists

  interrupted();
  EXPECT_TRUE(db.deleteList("people"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(PowerLossTest, InterruptedListStillShowsInShowLists) {
  SPIFFS.writeRaw(tmpPath("people"), "{\"name\":\"ann\"}\r\n");
  Serial.reset();
  Serial.feed("QUARKDB>show lists\n");
  db.processSerialCommand();
  EXPECT_NE(Serial.output().find("Lists found are..\npeople"), std::string::npos) << Serial.output();
}

TEST_F(PowerLossTest, DeleteListRemovesLeftoverTempFile) {
  seedPeople();
  SPIFFS.writeRaw(tmpPath("people"), "junk");
  EXPECT_TRUE(db.deleteList("people"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(PowerLossTest, FailedTempWriteLeavesListUntouched) {
  seedPeople();
  std::string original = SPIFFS.readRaw(listPath("people"));
  SPIFFS.failOpen("w");
  EXPECT_EQ(update("people", "{}", R"({"x":1})"), -1);
  EXPECT_EQ(db.deleteRecords("people", "{}"), -1);
  SPIFFS.failOpen("w", false);
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")), original);
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));
}

TEST_F(PowerLossTest, TornAppendDoesNotCorruptNextRecord) {
  ASSERT_TRUE(db.createList("log"));
  ASSERT_TRUE(add("log", R"({"name":"first"})"));
  // What flash holds after power loss halfway through appending a record.
  SPIFFS.writeRaw(listPath("log"), SPIFFS.readRaw(listPath("log")) + R"({"name":"to)");
  reboot();
  ASSERT_TRUE(add("log", R"({"name":"second"})"));
  EXPECT_EQ(findNames("log", "{}"), (Names{"first", "second"}));
  EXPECT_EQ(db.getRecordCount("log"), 2);
}

TEST_F(PowerLossTest, TornAppendIsCleanedUpByNextRewrite) {
  ASSERT_TRUE(db.createList("log"));
  ASSERT_TRUE(add("log", R"({"name":"first"})"));
  SPIFFS.writeRaw(listPath("log"), SPIFFS.readRaw(listPath("log")) + R"({"name":"to)");
  ASSERT_TRUE(add("log", R"({"name":"second"})"));
  ASSERT_EQ(db.deleteRecords("log", R"({"name":"first"})"), 1);
  EXPECT_EQ(SPIFFS.readRaw(listPath("log")), "{\"name\":\"second\"}\r\n");
}

TEST_F(PowerLossTest, PowerLossDuringAppendLosesOnlyThatRecord) {
  ASSERT_TRUE(db.createList("log"));
  ASSERT_TRUE(add("log", R"({"name":"first"})"));
  SPIFFS.powerLossAfterOpen("a");
  EXPECT_FALSE(add("log", R"({"name":"lost"})"));
  reboot();
  ASSERT_TRUE(add("log", R"({"name":"third"})"));
  EXPECT_EQ(findNames("log", "{}"), (Names{"first", "third"}));
}
