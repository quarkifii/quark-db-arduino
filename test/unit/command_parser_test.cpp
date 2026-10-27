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

// Tests for the serial command-line interface (QUARKDB> commands).
#include "test_helpers.h"

class CommandLineTest : public QuarkDBTest {
 protected:
  // Sends one line over the mocked Serial and returns everything printed.
  std::string cli(const std::string& line) {
    Serial.reset();
    Serial.feed(line + "\n");
    db.processSerialCommand();
    return Serial.output();
  }
  static bool has(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
  }
};

TEST_F(CommandLineTest, NoInputDoesNothing) {
  Serial.reset();
  db.processSerialCommand();
  EXPECT_EQ(Serial.output(), "");
}

TEST_F(CommandLineTest, RequiresPrefix) {
  EXPECT_TRUE(has(cli("db.create(\"x\")"), "Add \"QUARKDB>\" at start"));
}

TEST_F(CommandLineTest, EmptyLineIsRejected) {
  EXPECT_TRUE(has(cli(""), "Empty command"));
}

TEST_F(CommandLineTest, CreateAndDeleteList) {
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"people\")"), "Successfully created ->people"));
  EXPECT_TRUE(SPIFFS.exists(listPath("people").c_str()));
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"people\")"), "Failed to create ->people"));
  EXPECT_TRUE(has(cli("QUARKDB>db.delete(\"people\")"), "Successfully deleted ->people"));
  EXPECT_FALSE(SPIFFS.exists(listPath("people").c_str()));
  EXPECT_TRUE(has(cli("QUARKDB>db.delete(\"people\")"), "Failed to delete ->people"));
}

TEST_F(CommandLineTest, CreateValidation) {
  EXPECT_TRUE(has(cli("QUARKDB>db.create(people)"), "Enclose list name in double quotes"));
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"\")"), "Empty List Name"));
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"bad name\")"), "alphanumeric"));
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"x\""), "Use valid command"));
}

TEST_F(CommandLineTest, InvalidRoot) {
  EXPECT_TRUE(has(cli("QUARKDB>foo.create(\"x\")"), "ERROR:Invalid command -> foo"));
}

TEST_F(CommandLineTest, AddFindCount) {
  cli("QUARKDB>db.create(\"people\")");
  EXPECT_TRUE(has(cli("QUARKDB>db.people.add({\"name\":\"alice\",\"age\":30})"), "Successfully added to ->people"));
  EXPECT_TRUE(has(cli("QUARKDB>db.people.add({\"name\":\"bob\",\"age\":43})"), "Successfully added to ->people"));

  std::string count = cli("QUARKDB>db.people.count()");
  EXPECT_TRUE(has(count, "Total Count -> 2")) << count;

  std::string found = cli("QUARKDB>db.people.find({\"age\" : {\"$gt\" : 35}})");
  EXPECT_TRUE(has(found, "\"bob\"")) << found;
  EXPECT_FALSE(has(found, "\"alice\"")) << found;
  EXPECT_TRUE(has(found, "1/1 Records Retrieved")) << found;
}

TEST_F(CommandLineTest, AddValidation) {
  cli("QUARKDB>db.create(\"people\")");
  EXPECT_TRUE(has(cli("QUARKDB>db.people.add(\"x\")"), "Add valid json object"));
  EXPECT_TRUE(has(cli("QUARKDB>db.people.add({bad})"), "ERROR: Invalid json"));
  EXPECT_TRUE(has(cli("QUARKDB>db.ghost.add({\"a\":1})"), "Failed to add into ->ghost"));
}

TEST_F(CommandLineTest, FindOnMissingListReportsError) {
  EXPECT_TRUE(has(cli("QUARKDB>db.ghost.find({})"), "ERROR: Error reading list->ghost"));
  EXPECT_TRUE(has(cli("QUARKDB>db.ghost.count()"), "ERROR: Could not read list"));
}

TEST_F(CommandLineTest, UpdateCommand) {
  seedPeople();
  std::string out = cli(
      "QUARKDB>db.people.update({ \"filter\" : {\"age\" : 30} , \"updateObj\" : {\"age\" : 31 , \"name\" : \"ann\"} })");
  EXPECT_TRUE(has(out, "Results Updated Count [1]")) << out;
  EXPECT_EQ(findNames("people", R"({"age":31})"), (Names{"ann"}));
}

TEST_F(CommandLineTest, UpdateCommandRequiresFilterAndUpdateObj) {
  seedPeople();
  EXPECT_TRUE(has(cli("QUARKDB>db.people.update({\"filter\":{\"age\":30}})"), "Please pass filter and updateObj"));
  EXPECT_TRUE(has(cli("QUARKDB>db.ghost.update({\"filter\":{},\"updateObj\":{}})"), "ERROR: Could not update list"));
}

TEST_F(CommandLineTest, DeleteRecordsCommand) {
  seedPeople();
  std::string out = cli("QUARKDB>db.people.delete({\"active\":true})");
  EXPECT_TRUE(has(out, "Results Deleted Count [3]")) << out;
  EXPECT_EQ(db.getRecordCount("people"), 2);
}

TEST_F(CommandLineTest, ShowLists) {
  EXPECT_TRUE(has(cli("QUARKDB>show lists"), "No Lists Found"));
  cli("QUARKDB>db.create(\"alpha\")");
  cli("QUARKDB>db.create(\"beta\")");
  std::string out = cli("QUARKDB>show lists");
  EXPECT_TRUE(has(out, "Lists found are..\nalpha\nbeta")) << out;
}

TEST_F(CommandLineTest, ShowListsIgnoresForeignFiles) {
  SPIFFS.writeRaw("/config.json", "{}");
  cli("QUARKDB>db.create(\"alpha\")");
  std::string out = cli("QUARKDB>show lists");
  EXPECT_TRUE(has(out, "alpha"));
  EXPECT_FALSE(has(out, "config"));
}

TEST_F(CommandLineTest, SetMaxRecords) {
  EXPECT_TRUE(has(cli("QUARKDB>set max_records=10"), "Successfully set"));
  EXPECT_TRUE(has(cli("QUARKDB>set max_records=abc"), "ERROR:Invalid value"));
  EXPECT_TRUE(has(cli("QUARKDB>set max_record_size=900"), "Successfully set"));
  EXPECT_TRUE(has(cli("QUARKDB>set max_record_size=-1"), "ERROR:Invalid value"));
  cli("QUARKDB>db.create(\"people\")");
  EXPECT_TRUE(has(cli("QUARKDB>db.people.count()"), "max records set : 10, max single record size set : 900"));
}

TEST_F(CommandLineTest, SetMaxRecordsLimitsFindOutput) {
  seedPeople();
  cli("QUARKDB>set max_records=2");
  std::string out = cli("QUARKDB>db.people.find({})");
  EXPECT_TRUE(has(out, "2/5 Records Retrieved")) << out;
  EXPECT_TRUE(has(out, "\"bob\""));
  EXPECT_FALSE(has(out, "\"carol\""));
}

TEST_F(CommandLineTest, UnknownListOperation) {
  cli("QUARKDB>db.create(\"people\")");
  EXPECT_TRUE(has(cli("QUARKDB>db.people.drop({})"), "Invalid Command"));
}

TEST_F(CommandLineTest, ListNamesStartingWithKeywordsWork) {
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"createdItems\")"), "Successfully created ->createdItems"));
  EXPECT_TRUE(has(cli("QUARKDB>db.createdItems.add({\"a\":1})"), "Successfully added to ->createdItems"));
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"deleted\")"), "Successfully created ->deleted"));
  EXPECT_TRUE(has(cli("QUARKDB>db.deleted.add({\"a\":1})"), "Successfully added to ->deleted"));
  EXPECT_TRUE(has(cli("QUARKDB>db.deleted.count()"), "Total Count -> 1"));
  EXPECT_TRUE(has(cli("QUARKDB>db.deleted.delete({\"a\":1})"), "Results Deleted Count [1]"));
  EXPECT_EQ(db.getRecordCount("createdItems"), 1);
}

TEST_F(CommandLineTest, ListsNamedLikeKeywordsWork) {
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"create\")"), "Successfully created ->create"));
  EXPECT_TRUE(has(cli("QUARKDB>db.create.add({\"a\":1})"), "Successfully added to ->create"));
  EXPECT_EQ(db.getRecordCount("create"), 1);
  EXPECT_TRUE(has(cli("QUARKDB>db.delete(\"create\")"), "Successfully deleted ->create"));
}

TEST_F(CommandLineTest, ShowListsHidesTempFiles) {
  cli("QUARKDB>db.create(\"alpha\")");
  SPIFFS.writeRaw(tmpPath("alpha"), "");   // leftovers from an interrupted rewrite
  SPIFFS.writeRaw(newPath("alpha"), "");
  std::string out = cli("QUARKDB>show lists");
  EXPECT_TRUE(has(out, "Lists found are..\nalpha")) << out;
  EXPECT_FALSE(has(out, "alpha\nalpha")) << out;
}

TEST_F(CommandLineTest, CreateRejectsListNameLongerThan12) {
  EXPECT_TRUE(has(cli("QUARKDB>db.create(\"abcdefghijklm\")"), "1 to 12 alphanumeric"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}
