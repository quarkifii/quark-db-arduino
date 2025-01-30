#include <QuarkDB.h>

QuarkDB quarkDB;

int MAX_RECORD_SIZE = 300;

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
// Please note for precision match we need to follow serialization so we do rounding based on our need .. here 2 decimal point
// Please refer https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats/
double round2(float value) {
   return (int)(value * 100 + 0.5) / 100.0;
}
// First we create alist
// Then we add fixed values
// Then we retrieve and match
void loop() {
  // creating a list:
  Serial.println("Creating List..");
  bool cStatus = quarkDB.createList("testList");
  if(cStatus) {
    Serial.println("Successfully added List");
  }else {
    Serial.println("Failed To add");
  }
  //Sample data to test with
  String nameArray[5]= {"test-0" , "test-1", "test-2", "test-3", "test-4"};
  float humidityArray[5]= {73.4 , 74.3, 75.3, 76.1, 75.9};
  float innerArray[5]= {53.4 , 44.3, 28.3, 56.1, 75.9};


  // Adding records to the list with inner array object
  for(int i=0 ; i < 5; i++) {
      JsonDocument doc;
      doc["name"] = nameArray[i];
      doc["humidity"] = humidityArray[i];
      if(i%2 == 0) {
        JsonArray innerObjArray = doc["innerArray"].to<JsonArray>();
        for(int j =0; j < 5; j++) {
          innerObjArray.add(innerArray[j] + i);
        }
      }
      bool statusA = quarkDB.addRecord("testList", doc.as<JsonObject>());
      if(statusA) {
        Serial.printf("Successfully added %d\n",(i+1));
      }else {
        Serial.printf("ERROR:Failed To add  %d\n " , (i+1));
      }
  }
  int totalCount = quarkDB.getRecordCount("testList");
  if(totalCount == 5) {
    Serial.println("Record Count Matched successfully");
  }else {
    Serial.println("ERROR: Record Count not retrieved successfully");
  }
  JsonDocument results;
  //Get All records with empty filter
  int count = quarkDB.getRecords("testList" , "{}" , &results);
  Serial.printf("%d Records retrieved successfully\n",count);
  if(count == 5) {
    Serial.println("All records retrieved successfully");
  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }
  serializeJson(results, Serial);
  Serial.println("");
  if(results.is<JsonArray>()) {
    int i = 0;
    for (JsonVariant arrayItem : results.as<JsonArray>()) {
      Serial.printf("Matching Record - %d\n" , (i+1));
      //serializeJson(arrayItem, Serial);
      //Serial.println(arrayItem);
      if(arrayItem["innerArray"].is<JsonArray>()) {
        JsonArray innerJsonArray = arrayItem["innerArray"].as<JsonArray>();
        byte matchCount=0;
        // Please note for precision match we need to follow serialization so we do rounding based on our need .. here 2 decimal point
        // Please refer https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats/
        for(int j =0 ; j < innerJsonArray.size() ; j ++) {
          if(round2(innerJsonArray[j].as<float>()) == round2(innerArray[j] + i)) matchCount++;
        }
        if(matchCount == 5) {
          Serial.printf("Inner Array OK \t|");
        }else {
          Serial.printf("ERROR:Inner Array NOK \t|");
        }

      }else{
        Serial.printf("No Inner Array for record %d \t|" , i);
      }
      i++;
      Serial.println("");
    }
  }else{
    Serial.println("ERROR: Data not retireived correctly");
  }

  JsonDocument resultSingle;
  //Get a records with filter for specific array value to select the entire record using $eleMatch operand
  int countS = quarkDB.getRecords("testList" , "{\"innerArray\" : { \"$eleMatch\" : { \"$eq\" : 53.4 } } }" , &resultSingle);
  Serial.printf("%d Records retrieved successfully\n",countS);
  if(countS == 1) {
    Serial.println("Element matched Record retrieved successfully");
  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }
  serializeJsonPretty(resultSingle, Serial);
  Serial.println("");
  if(resultSingle.is<JsonArray>()) {
      JsonArray  innerArrayCheck =  resultSingle.as<JsonArray>()[0]["innerArray"].as<JsonArray>();
      byte matchCount=0;
      for(int j =0 ; j < innerArrayCheck.size() ; j ++) {
          if(round2(innerArrayCheck[j].as<float>()) == round2(innerArray[j])) matchCount++;
        }
        if(matchCount == 5) {
          Serial.printf("Inner Array OK \t|");
        }else {
          Serial.printf("ERROR:Inner Array NOK \t|");
        }
      Serial.println("");
  }else{
    Serial.println("ERROR: Data not retrieved correctly");
  }

  Serial.println("Deleting list now");
  bool dStatus = quarkDB.deleteList("testList");
  if(dStatus) {
    Serial.println("Successfully deleted List");
  }else {
    Serial.println("Failed To delete");
  }
  delay(3000);


   // Uncomment below to process command line interface
  //quarkDB.processSerialCommand();
}
