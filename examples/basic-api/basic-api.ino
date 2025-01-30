#include <QuarkDB.h>

QuarkDB quarkDB;
//This is the max single record/document size in quarkDB
//QuarkDB default support is 500 bytes
//If you need more than that you need to use setMaxRecordSize api  after considering your heap space analysis
//In this example we are using record size way below the default max limit

int MAX_RECORD_SIZE = 110;

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
  int ageArray[5]= {30,43,46,24,36};
  float tempArray[5]= {23.4f , 24.8f, 25.8f, 26.1f, 25.9f};
  float humidityArray[5]= {73.4f , 74.8f, 75.8f, 76.1f, 75.9f};

  // Adding records to the list
  for(int i=0 ; i < 5; i++) {
      JsonDocument doc;
      doc["name"] = nameArray[i];
      doc["age"] = ageArray[i];
      doc["temp"] = tempArray[i];
      doc["humidity"] = humidityArray[i];
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
  //Checking results as per fed data to the db
  if(results.is<JsonArray>()) {
    int i = 0;
    for (JsonVariant arrayItem : results.as<JsonArray>()) {
      Serial.printf("Matching Record - %d\n" , (i+1));
      if(arrayItem["name"] == nameArray[i]) {
        Serial.printf("Name OK \t|");
      }else{
        Serial.printf("ERROR:Name NOK \t|");
      }
      if(arrayItem["age"] == ageArray[i]) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      // Please note for precision match we need to follow serialization so we do rounding based on our need .. here 2 decimal point
      // Please refer https://arduinojson.org/v6/how-to/configure-the-serialization-of-floats/
      if(round2(arrayItem["temp"].as<float>()) == round2(tempArray[i])) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK %f %f\t|", arrayItem["temp"].as<float>(), tempArray[i]);
      }
      if(round2(arrayItem["humidity"].as<float>()) == round2(humidityArray[i])) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");

      }
      i++;
      Serial.println("");
    }
  }else{
    Serial.println("ERROR: Data not retrieved correctly");
  }
  //Updating  data
  JsonDocument updateDoc;
  updateDoc["name"] = "test-0";
  updateDoc["age"] = 33;
  updateDoc["temp"] = 27.8;
  updateDoc["humidity"] = 56.3;
  //pass the list name, matching selector and the new object to replace for all matched records
  int updateCount = quarkDB.updateRecords("testList", "{\"age\" : 30 }" , updateDoc.as<JsonObject>());
  if(updateCount == 1) {
    Serial.println("Record updated successfully");
  }else {
    Serial.println("ERROR: Record not updated successfully");
  }
  //Retrieve and check after update
  JsonDocument resultUpdate;
  //Get All records with filter specifying the update filter
  int countUpd = quarkDB.getRecords("testList" , "{\"age\" : 33 }" , &resultUpdate);
  if(countUpd == 1) {
    Serial.println("Record retrieved successfully");
  }else {
    Serial.println("ERROR: Record not retrieved successfully");
  }
  Serial.println("****************Matching updated record now***************");
  serializeJson(resultUpdate, Serial);
   if(countUpd == 1) {
    JsonObject jobj = resultUpdate.as<JsonArray>()[0];
    if(jobj["name"] == "test-0") {
        Serial.printf("Name OK \t|");
      }else{
        Serial.printf("ERROR:Name NOK \t|");
      }
      if(jobj["age"] == 33) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      if(round2(jobj["temp"].as<float>()) == round2(27.8f)) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }
      if(round2(jobj["humidity"].as<float>()) == round2(56.3f)) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");

      }

   }
  Serial.println("");
  // Checking deleting row now.. pass the ist name and the matching selector to delete all matched records
  quarkDB.deleteRecords("testList", "{\"age\" : 33 }" );
  if(updateCount == 1) {
    Serial.println("Record deleted successfully");
  }else {
    Serial.println("ERROR: Record not deleted successfully");
  }
  JsonDocument resultDelete;
  //Get All records with filter used to delete records. should return empty now
  int countDel = quarkDB.getRecords("testList" , "{\"age\" : 33 }" , &resultDelete);
  if(countDel == 0 && resultDelete.as<JsonArray>().size() == 0) {
    Serial.println("Record retrieved successfully after deletion");
  }else {
    Serial.println("ERROR: Record not retrieved successfully after deletion");
  }

  Serial.println("Deleting list now");
  bool dStatus = quarkDB.deleteList("testList");
  if(dStatus) {
    Serial.println("Successfully deleted List");
  }else {
    Serial.println("Failed To delete");
  }
  delay(3000);

  //quarkDB.processSerialCommand();
}
