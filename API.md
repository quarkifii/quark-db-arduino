


# QuarkDB API and Command Line Details

## Include Library

```arduino 
#include  <QuarkDB.h> 
```   
## Setup
Make sure Arduino IDE is setup with ESP8266/ESP32 development configurations
### Begin

Initialize the library with the specified file type (QUARKDB_SPIFFS_FILE_TYPE is supported now) .

```arduino 
bool status = quarkDB.init(QUARKDB_SPIFFS_FILE_TYPE);  
if(status)  {  
 Serial.println("QuarkDB initialized");  
} 
``` 
Returns `true` on success, `false` on failure.


### Set Command Line Interface to process CLI Commands

Initialize the Serial Interface
```arduino 
Serial.begin(115200);  
```  
Add the below line in loop() function.

```arduino 
void  loop()  {  
 quarkDB.processSerialCommand();
}  
```
* This will process serial commands sent using serial monitor

## DB Operations
By default QuarkDB assumes each json record of 500 bytes maximum.  
By default QuarkDB assumes maximum 300 records in a list.  
Above defaults can be modified by relevant APIs as defined below

### Create a DB List
#### Using API
```arduino
bool cStatus = quarkDB.createList("testMyList");  
if(cStatus)  {  
  Serial.println("Successfully added List");  
}else  {  
 Serial.println("Failed To add");  
} 
```
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.create("testMyList")
out# Successfully created ->testMyList  

``` 
### Add Element to a DB List
#### Using API
```arduino 
DynamicJsonDocument doc(MAX_RECORD_SIZE);  
doc["age"] = 23;  
doc["name"] = "John";  
bool statusA = quarkDB.addRecord("testMyList", doc.as<JsonObject>());  
if(statusA) {  
  Serial.printf("Successfully added \n");
 }else {  
  Serial.printf("ERROR:Failed To add \n ");
}  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.add({"age" : 23 , "name" : "John"}) 
out# writing record {"age":23,"name":"John"}
Successfully added to ->testMyList 
``` 
### Get Total Record Count of a DB List
#### Using API
```arduino 
int totalCount = quarkDB.getRecordCount("testMyList");  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.count()  
out# Total Count -> 1 [max records set : 300, max single record size set : 500 ]
```  
### Comparison Operators Supported for Data Retrieval
```
$eq - Equality  
$gt - Greater Than  
$lt - Lesser Than  
$gte - Greter than or equal to  
$lte - Less than or equal to  
$eleMatch - Matching with array element  
```  
### Get Records from a DB List with empty filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with empty filter  
int count = quarkDB.getRecords("testMyList" , "{}" , &results);  
serializeJson(results, Serial);  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({})  
out# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with basic equality filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : 23 }" , &results);  
serializeJson(results, Serial);  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"age" : 23})  
out# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with basic greater than filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$gt\" : 22 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"age" : {"$gt" : 22 }})  
out# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with basic greater than or equal filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$gte\" : 22 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"age" : {"$gte" : 22 }})  
out# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with basic less than filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$lt\" : 25 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Using Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"age" : {"$lt" : 25 }})  
out# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with basic less than or equal filter
#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$lte\" : 25 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"age" : {"$lte" : 25 }})  
ou# [

  {

    "age": 23,

    "name": "John"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Get Records from a DB List with matching element within an array
Suppose elements are as follows  
[  
{ innerArray : [1,2,3,4] , id : "test1"},  
{ innerArray : [5,6,7,8] , id : "test2"},  
]

#### Using API
Results are returned in a reference of DynamicJsonDocument passed
```arduino 
DynamicJsonDocument results(totalCount*MAX_RECORD_SIZE);  
//Get All records with records having inner array one element equal to 2  
int count = quarkDB.getRecords("testMyList" , "{\"innerArray\"  : { \"$eleMatch\": { \"$eq\" : 2 } } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.find({"innerArray" : { "$eleMatch" : {"$eq" : 2 }}})  
out# [

  {

    "innerArray": [

      1,

      2,

      3,

      4

    ],

    "id": "test1"

  }

]

1/1 Records Retrieved [max records set : 300 bytes, max single record size set : 500bytes ]
```  
### Update Record
#### Using API
Pass the filter to match records and the new updated object to replace matched records. Partial update is not supported now
```arduino 
DynamicJsonDocument updateDoc(MAX_RECORD_SIZE);  
updateDoc["name"] = "Alter";  
updateDoc["age"] = 43;  
//pass the list name, matching selector and the new object to replace for all matched records  
int updateCount = quarkDB.updateRecords("testMyList", "{\"age\" : 23 }" , updateDoc.as<JsonObject>());  
``` 
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.update({ "filter" : {"age" : 23} , "updateObj" : {"age" : 43 , "name" : "Alter"} })
out#  updating object1

Results Updated Count [1]  
```  
### Delete Record
#### Using API
Pass the filter to match records and delete
```arduino 
int deleteCount = quarkDB.deleteRecords("testMyList", "{\"age\" : 43 }"  );  
``` 
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.testMyList.delete({"age" : 43})  
out# Results Deleted Count [1]
```  
### Delete List
#### Using API
Pass the list name to delete
```arduino 
bool dStatus = quarkDB.deleteList("testMyList");  
if(dStatus)  {  
  Serial.println("Successfully deleted List");
 }else{  
 Serial.println("Failed To delete");
 }  
``` 
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.delete("testMyList")  
out# Successfully deleted ->testMyList
```  
### Update Max Size for each Record
#### Using API
Pass the size in bytes
```arduino 
quarkDB.setMaxRecordSize(600);  
``` 
#### Command Line using Serial Monitor
```  
QUARKDB>set max_record_size=600  
```  
### Update Max Records for a List
#### Using API
```arduino 
quarkDB.setMaxRecords(600);  
``` 
#### Command Line using Serial Monitor
```  
QUARKDB>set max_records=600  
```
For any issue please reach out to support@quarkifi.com or raise an issue