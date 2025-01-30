#include <QuarkDB.h>

QuarkDB quarkDB;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  bool status = false;
  short timeout = 0;
  //Wait for max 30 secs init
  while(!status && (++timeout) < 3000) {
    status = quarkDB.init(QUARKDB_SPIFFS_FILE_TYPE);
    delay(10);
  }
  if(status) {
    Serial.println("QuarkDB initialized");
  }
}
// This will allow command line interface commands to be processed and executed..
// Open Serial Monitor and execute these commands to have a command line interface with the db
// Send the serial commands terminated with crlf(newline)
// See examples below
void loop() {
  quarkDB.processSerialCommand();
}
//examples
// command to show all lists
//QUARKDB>show lists
// command to create list
//QUARKDB>db.create("testMyList")
// command to add simple objects to lists
// use db.<listName>.command
//QUARKDB>db.testMyList.add({"t1" : 21.4})
//QUARKDB>db.testMyList.add({"t1" : 21.4 , "t3" : 32.2})
//QUARKDB>db.testMyList.add({"t1" : 23.3 , "t2" : 89.7})
// command to add complex objects to lists
//QUARKDB>db.testMyList.add({"t1" : {"inner" : [2,4,3,2]}  , "t2" : 44 , "t3" : 54.32})
//QUARKDB>db.testMyList.add({"t1" : {"inner" : [6,8,9,3]}  , "t2" : 66})
// command to do total count
//QUARKDB>db.testMyList.count()
// command to do simple query.
//QUARKDB>db.testMyList.find({"t1" : 21.4})
// command to do simple query.
//QUARKDB>db.testMyList.find({"t1" : 21.4 , "t3" : 32.2})
// command to do query with greater than or equal operand $gte
//QUARKDB>db.testMyList.find({"t1" : { "$gte" : 21 } })
// command to do query with less than or equal operand $lte
//QUARKDB>db.testMyList.find({"t1" : { "$lte" : 24 } })
// command to do query with array element match
//QUARKDB>db.testMyList.find({"t1.inner" : { "$eleMatch" : {"$eq" : 6} } })
//QUARKDB>db.testMyList.find({"t1.inner" : { "$eleMatch" : {"$lte" : 3} } })
// command to do update with matching filter
//QUARKDB>db.testMyList.update({ "filter" : {"t1" : 23.3} , "updateObj" : {"t1" : 23.3 , "t2" : 99.7} })
// command to do delete with matching filter
//QUARKDB>db.testMyList.delete({"t1" : 23.3})
// command to do delete list
//QUARKDB>db.delete("testMyList")


