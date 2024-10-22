#include <QuarkDB.h>

QuarkDB quarkDB;

int MAX_RECORD_SIZE = 110;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  bool status = quarkDB.init(QUARKDB_SPIFFS_FILE_TYPE);
  if(status) {
    Serial.println("QuarkDB initialized");
  }
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
  int ageArray[5]= {30,43,46,24,33};
  float tempArray[5]= {23.4 , 24.3, 25.3, 26.1, 25.9};
  float humidityArray[5]= {73.4 , 74.3, 75.3, 76.1, 75.9};

  // Adding records to the list
  for(int i=0 ; i < 5; i++) {
      DynamicJsonDocument doc(MAX_RECORD_SIZE);
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
  DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);
  Serial.println("************* Matching temp value ******************");
  //Get Single record with single match of temp value
  int count = quarkDB.getRecords("testList" , "{\"temp\" : 23.4}" , &results);
  Serial.printf("%d Records retrieved successfully\n",count);
  serializeJson(results, Serial);
  Serial.println("");
  if(count == 1) {
    Serial.println("Record retrieved successfully");
    JsonObject jobj = results.as<JsonArray>()[0];
    if(jobj["temp"].as<float>() == 23.4f) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }

  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }
  Serial.println("");
  Serial.println("************* Matching multiple json value ******************");
  DynamicJsonDocument resultSingle(MAX_RECORD_SIZE);
  //Get Single record with with multiple match values
  int countMl = quarkDB.getRecords("testList" , "{\"temp\" : 23.4 , \"age\" : 30 , \"humidity\" : 73.4}" , &resultSingle);
  Serial.printf("%d Records retrieved successfully\n",countMl);
  serializeJson(resultSingle, Serial);
  Serial.println("");
  if(countMl == 1) {
    Serial.println("Record retrieved successfully");
    JsonObject jobj = resultSingle.as<JsonArray>()[0];
    if(jobj["temp"].as<float>() == 23.4f) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }
      if(jobj["age"] == 30) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      if(jobj["humidity"].as<float>() == 73.4f) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");
      }

  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }
  Serial.println("");
  Serial.println("************* Matching temp with greater than or equal value ******************");
  DynamicJsonDocument resultMulti(5*MAX_RECORD_SIZE);
  //Get 2 records with greater than or equal to operand "$gte"
  int countGMl = quarkDB.getRecords("testList" , "{\"temp\" : { \"$gte\" : 25.4 } }" , &resultMulti);
  Serial.printf("%d Records retrieved successfully\n",countGMl);
  serializeJson(resultMulti, Serial);
  Serial.println("");
  if(countGMl == 2) {
    Serial.println("Record retrieved successfully");
    {
      Serial.println("Matching first selected record");
      JsonObject jobj = resultMulti.as<JsonArray>()[0];
      if(jobj["temp"].as<float>() == 26.1f) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }
      if(jobj["age"] == 24) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      if(jobj["humidity"].as<float>() == 76.1f) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");
      }

    }
    Serial.println("");
    {
      Serial.println("Matching second selected record");
      JsonObject jobj = resultMulti.as<JsonArray>()[1];
      if(jobj["temp"].as<float>() == 25.9f) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }
      if(jobj["age"] == 33) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      if(jobj["humidity"].as<float>() == 75.9f) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");
      }

    }
    

  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }

  Serial.println("");
   Serial.println("************* Matching with less than or equal value on mltiple elements******************");
  DynamicJsonDocument resultNSingle(5*MAX_RECORD_SIZE);
    //Get Single record with less than or equal to operand "$lte"
  int countNMl = quarkDB.getRecords("testList" , "{\"temp\" : { \"$lte\" : 23.4 } ,\"age\" : { \"$lte\" : 30 }  }" , &resultNSingle);
  Serial.printf("%d Records retrieved successfully\n",countNMl);
  serializeJson(resultNSingle, Serial);
  Serial.println("");
  if(countNMl == 1) {
      JsonObject jobj = resultNSingle.as<JsonArray>()[0];
      if(jobj["temp"].as<float>() == 23.4f) {
        Serial.printf("Temp OK \t|");
      }else{
        Serial.printf("ERROR:Temp NOK \t|");
      }
      if(jobj["age"] == 30) {
        Serial.printf("Age OK \t|");
      }else{
        Serial.printf("ERROR:Age NOK \t|");
      }
      if(jobj["humidity"].as<float>() == 73.4f) {
        Serial.printf("Humidity OK \t|");
      }else{
        Serial.printf("ERROR:Humidity NOK \t|");
      }
    
  }else {
    Serial.println("ERROR: All records not retrieved successfully");
  }

  Serial.println("");
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
