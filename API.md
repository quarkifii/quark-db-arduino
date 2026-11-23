


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
List name must be below 12 characters
```arduino
bool cStatus = quarkDB.createList("testMyList");  
if(cStatus)  {  
  Serial.println("Successfully added List");  
}else  {  
 Serial.println("Failed To add");  
} 
```
#### Command Line using Serial Monitor
```  
cmd# QUARKDB>db.create("testMyList")
out# Successfully created ->testMyList  

``` 
### Add Element to a DB List
#### Using API
```arduino 
JsonDocument doc;  
doc["age"] = 23;  
doc["name"] = "John";  
bool statusA = quarkDB.addRecord("testMyList", doc.as<JsonObject>());  
if(statusA) {  
  Serial.printf("Successfully added \n");
 }else {  
  Serial.printf("ERROR:Failed To add \n ");
}  
``` 
#### Command Line using Serial Monitor
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
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
//Get All records with empty filter  
int count = quarkDB.getRecords("testMyList" , "{}" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : 23 }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$gt\" : 22 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$gte\" : 22 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
//Get All records with records having age equal to 33  
int count = quarkDB.getRecords("testMyList" , "{\"age\" : { \"$lt\" : 25 } }" , &results);  
serializeJson(results, Serial);  
``` 
#### Command Line using Serial Monitor
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
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
Results are returned in a reference of JsonDocument passed
```arduino 
JsonDocument results;  
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
JsonDocument updateDoc;  
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
## Cloud Backup over MQTT
The device copy of a list is the master. The cloud keeps a backup snapshot
of every list with sync enabled and applies the deltas the device publishes.
Changes made while offline accumulate in a per list **oplog** on flash and
are sent on the next connection. The device can also pull the cloud copy
back, on demand or automatically after a flash wipe.

The cloud side (a service that stores the snapshot and ops and answers the
device) is out of scope of this library; the message formats it has to speak
are listed below. A full sketch is in `examples/cloud-sync`.

### Include
```arduino
#include <PubSubClient.h>        // https://github.com/knolleary/pubsubclient
#include <QuarkDB.h>
#include <QuarkMqttTransport.h>  // QuarkTransport over PubSubClient (optional header)
```
`QuarkMqttTransport.h` is not included by `QuarkDB.h`, so PubSubClient is
only needed by sketches that use cloud backup. Any other MQTT client can be
used by implementing the `QuarkTransport` interface from `QuarkTransport.h`
(`connected()`, `publish()`, `subscribe()`, optional `maxPayloadSize()`);
`QuarkDB.h` includes it together with the sync configuration types
(`QuarkSyncConfig`, `QuarkListSyncOptions`, `QuarkRestoreMode`).

### Enable Sync
```arduino
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
QuarkMqttTransport transport(mqtt);   // QuarkMqttTransport(mqtt, /*retained=*/false)

QuarkSyncConfig syncConfig;
syncConfig.publishTopic = "devices/esp-1/quarkdb/out";   // device -> cloud
syncConfig.subscribeTopic = "devices/esp-1/quarkdb/in";  // cloud -> device (MQTT wildcards allowed)
syncConfig.deviceId = "esp-1";            // optional, added as "dev" to every message
syncConfig.maxOplogBytes = 4096;          // per list oplog cap on flash (default 4096)
syncConfig.maxPayloadBytes = 1024;        // largest MQTT payload built (default 1024)
syncConfig.restoreOnMismatch = false;     // default restore policy (see below)
syncConfig.queryTimeoutMs = 30000;        // re-query on the next change if the cloud never answered (0 = never)
// limits against a misbehaving cloud (defaults shown)
syncConfig.maxInboundBytes = 1024;        // longer inbound messages are dropped unparsed
syncConfig.maxRestoreBytes = 65536;       // a restore staging more than this is aborted
syncConfig.restoreTimeoutMs = 60000;      // a restore whose chunks stop coming is abandoned
syncConfig.minCloudRequestIntervalMs = 5000;  // cloud "request" messages served at most once per interval per list

mqtt.setBufferSize(syncConfig.maxPayloadBytes + 64);   // PubSubClient defaults to 256 bytes
mqtt.setCallback([](char* topic, uint8_t* payload, unsigned int length) {
  quarkDB.handleMessage(topic, payload, length);
});

bool status = quarkDB.enableSync(&transport, syncConfig);
```
Returns `false` if the transport is `NULL`, a topic is empty or a byte limit
is below 64. `disableSync()` detaches the transport; changes keep being
logged. `isSyncEnabled()` tells whether a transport is attached.

`maxPayloadBytes` is capped further by what the transport can carry
(`QuarkMqttTransport` reports the PubSubClient buffer size minus the topic
and header). A single record must fit in it.

### Enable Sync for a List
```arduino
quarkDB.enableListSync("config");                      // inherit restoreOnMismatch
QuarkListSyncOptions options;
options.restore = QUARK_RESTORE_ON_MISMATCH;            // or QUARK_RESTORE_NEVER, QUARK_RESTORE_INHERIT
quarkDB.enableListSync("telemetry", options);
```
The list must exist. The first call creates `/__quarkdb__/<list>.sync` with
a random **instance id** that identifies this incarnation of the list to the
cloud. From then on every `addRecord()`, `updateRecords()`,
`deleteRecords()` and `deleteList()` (API or CLI) appends an entry with a
**sequence number** and the operation type to `/__quarkdb__/<list>.oplog`.
Sync settings persist on flash, so after a reboot changes are logged before
`enableListSync()` is called again; calling it again only updates the
options. `enableSync()` is not required for logging, only for publishing.

Each list name also remembers, in `.sync`, the **history** of instance ids
this device deleted on purpose with `deleteList()` (the newest 8) and a
**lost** flag, set when `createList()` runs while the metadata still holds a
live instance, i.e. the list file disappeared without `deleteList()`
(corruption, manual removal). With that, the cloud's answer to a `query` is
classified and the restore option only applies where a restore is actually
the right thing:

| cloud holds | meaning | device does |
|-------------|---------|-------------|
| the current instance | normal | sends the missing ops (or a snapshot after a rollover) |
| the current instance, local list `lost` | data lost before a delete | restores when allowed, otherwise pushes a snapshot |
| an instance in the history | deleted on purpose, cloud was never told | never restores: pushes a snapshot (or `drop` if the list no longer exists) |
| an instance it never issued | no memory of it: flash wiped (or another device's copy) | restores when allowed, otherwise pushes a snapshot |
| nothing | | pushes a snapshot |

The restore option:
* `QUARK_RESTORE_NEVER` (the default through `restoreOnMismatch = false`):
  the device is master; the cloud copy is replaced by a snapshot.
* `QUARK_RESTORE_ON_MISMATCH`: pull the cloud copy in the two cases above
  and replace the local list with it, adopting the cloud instance id and
  sequence number.
* `QUARK_RESTORE_INHERIT`: use `QuarkSyncConfig::restoreOnMismatch`.

The history is cleared as soon as the cloud is known to hold the current
instance (it holds one copy per name, so older ids can no longer come back).
`restoreFromCloud()` is explicit and always honoured.

```arduino
bool on = quarkDB.isListSyncEnabled("config");
quarkDB.disableListSync("config");   // removes the oplog and metadata; the cloud copy is left as is
```

### Sync on Connection
```arduino
if (mqtt.connect("esp-1")) {
  int lists = quarkDB.syncCloud();
}
```
Call it every time the MQTT connection is (re)established. It subscribes to
`subscribeTopic` and publishes a `query` for every synced list; the cloud
answers with a `status` and the device then sends what is missing. Returns
the number of lists queried, `-1` when offline or sync is not enabled.
Lists deleted while offline are queried too (with an empty `inst`): if the
cloud still holds an instance from the history the device sends a `drop`
for it. A list waiting for restore chunks is left alone.

While the connection is up every change is published right away (after a
`query`/`status` round trip if the cloud state is not known yet). Changes
made while offline, or whose publish failed, stay in the oplog and are sent
on the next `syncCloud()` or with the next change.

Nothing blocks: after `syncCloud()` a list is in state `queried` and changes
made before the `status` arrives are only logged, never pushed blindly. The
answer comes in through `handleMessage()` (i.e. `mqtt.loop()`), and the
accumulated entries go out in order then. If no answer arrives within
`queryTimeoutMs`, the next change sends a new `query` instead of waiting
for the next `syncCloud()`.

Instance ids are exchanged on every handshake: the `query` carries the
device's id, the `status` the cloud's. After a flash wipe the recreated list
gets a new id, the cloud still answers with the old one, and that mismatch
is what decides between pulling the backup (`QUARK_RESTORE_ON_MISMATCH`,
the device then adopts the cloud id and sequence number) and pushing the
new, empty list (`QUARK_RESTORE_NEVER`).

### Handle Cloud Messages
```arduino
void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (quarkDB.handleMessage(topic, payload, length)) return;   // a sync message
  // ... other topics
}
```
`handleMessage(const char* topic, String payload)` is available too. Returns
`true` if the message was on the subscribe topic, well formed, for a synced
list and handled. Messages for other devices (a `dev` field that differs from
`deviceId`) are ignored.

### Defensive Behaviour
The device is the only party that starts an exchange. Nothing loops, polls
or retries on its own: every outbound message is caused by a local write,
by `syncCloud()`, or by exactly one inbound message the device was waiting
for. Against a misbehaving cloud:
* a `status` is only acted on while the list is waiting for one (state
  `queried`) and only if it carries the `mid` of that query, and only once;
  anything else is ignored. An `ack` can only move the known cloud position
  forward, only for a `mid` the device published, and never sends anything;
* a `request` is served at most once per `minCloudRequestIntervalMs` per
  list, never during a restore, and a `lost` list answers no request or ack
  at all (it needs the `query`/`status` decision first);
* `snapshot` chunks are only accepted while a restore is pending, in order,
  with an instance id, up to `maxRestoreBytes`, and within
  `restoreTimeoutMs` of the request, and the staged copy replaces the list
  only when the `crc`/`count` of the last chunk match; anything else is
  refused and the staging file is removed, the local list untouched. A timed out restore is
  not retried (call `restoreFromCloud()` again if wanted);
* inbound payloads longer than `maxInboundBytes` are dropped before they
  are parsed; messages for unknown or invalid list names, unknown types, or
  another `dev` are ignored;
* the only unsolicited re-send is one `query` per `queryTimeoutMs`, and only
  when a local change has something to sync.

### Restore from Cloud
```arduino
int status = quarkDB.restoreFromCloud("config");   // 1 requested, 0 offline, -1 list not synced
```
Publishes a `restore` request; the cloud answers with `snapshot` chunks that
`handleMessage()` stages in `/__quarkdb__/<list>.rst`. The last chunk must
carry the integrity check of the whole snapshot, `crc` and `count` (see
Messages); the device computes both while staging and **only installs the
staged file** (with the same crash-safe swap as updates) **when they
match**. A mismatch or a missing checksum discards the staged copy and
leaves the local list untouched. Local changes made while waiting are
discarded together with the oplog once installed. Snapshots that were not
requested are ignored.

### Sync Status
```arduino
JsonDocument info;
if (quarkDB.getSyncInfo("config", &info)) {
  // info["inst"], info["seq"], info["cloudSeq"], info["state"] ("unknown" | "queried" | "insync" | "restoring"),
  // info["pending"] (oplog entries the cloud does not have), info["restore"], info["history"], info["lost"],
  // info["queryMid"] / info["restoreMid"] (open request the device waits an answer for, 0 = none), info["syncEnabled"]
}
```

### Oplog and Rollover
* `/__quarkdb__/<list>.sync`: `{"inst":"...","seq":N,"restore":R,"hist":"id1,id2","lost":1}`
* `/__quarkdb__/<list>.oplog`: one line per change, oldest first
  ```
  {"seq":1,"op":"add","doc":{"name":"alice","age":30}}
  {"seq":2,"op":"update","sel":{"age":30},"doc":{"name":"alice","age":31},"n":1}
  {"seq":3,"op":"delete","sel":{"name":"bob"},"n":1}
  {"seq":4,"op":"drop"}
  ```
* When an append would exceed `maxOplogBytes` the oldest entries are dropped
  (rollover). If the cloud later reports a sequence number that is no longer
  in the oplog, or any other gap is found, the device sends the current
  snapshot instead of the deltas. An entry larger than the cap is numbered
  but not stored, which also results in a snapshot.
* The sequence counter in `.sync` is only refreshed every 32 changes (and
  whenever an entry could not be stored); the newest oplog line carries the
  real value and the counter is reconciled from it on load, so a change
  costs one extra flash write, not two.

### Messages
All messages are JSON objects with `type`, `list`, `inst` (and `dev` when
configured). `maxPayloadBytes` applies to every message the device sends.

Every message the device publishes carries a numeric **message id** `mid`
(unique per message, starting at a random value on each boot). The cloud
correlates its answers to it, which is what keeps late or reordered answers
from being applied to the wrong request:
* a `status` must carry the `mid` of the query it answers; only the answer
  to the query the device is currently waiting for is acted on, and only
  once. A late answer to an earlier query is ignored;
* every `snapshot` chunk answering a `restore` must carry the `mid` of that
  `restore`; chunks of a superseded request are refused;
* an `ack` names the `mid` of the `op` (or final `snapshot` chunk) it
  acknowledges. The device maps it back to the sequence number that message
  carried (it keeps the last 8 published messages per list) and moves the
  known cloud position forward only, so acks can arrive in any order;
  unknown, repeated or older mids change nothing.

Device to cloud, on `publishTopic`:
| Message | Meaning |
|---------|---------|
| `{"type":"query","list":"config","inst":"a1","seq":12,"mid":101}` | What does the cloud hold for this list? `inst` is `""` for a list the device deleted. |
| `{"type":"op","list":"config","inst":"a1","seq":13,"mid":102,"op":"add","doc":{...}}` | One oplog entry. `op` is `add` (`doc`), `update` (`sel`, `doc`, `n`), `delete` (`sel`, `n`) or `drop` (the list was deleted; `inst` is the deleted instance). `sel` is the selector the cloud has to apply with the same matching rules as `getRecords()`. |
| `{"type":"snapshot","list":"config","inst":"a1","seq":13,"mid":103,"i":0,"last":true,"docs":[...],"crc":3421780262,"count":7}` | Full copy in chunks of at most `maxPayloadBytes`, each with its own `mid`. Chunk `0` replaces the cloud copy; the chunk with `last` commits it at `seq` and carries the integrity check: `count` documents in total and `crc`, the CRC-32 (IEEE, same as zlib's `crc32()`) over every document in compact JSON form followed by `\n`, in order, across all chunks. |
| `{"type":"restore","list":"config","inst":"a1","seq":13,"mid":104}` | Please send your copy as `snapshot` chunks carrying this `mid`. |

Cloud to device, on `subscribeTopic`:
| Message | Meaning |
|---------|---------|
| `{"type":"status","list":"config","inst":"a1","seq":10,"mid":101}` | Answer to the `query` with that `mid`, acted on only while the device waits for it. `inst` `""` means the cloud has no copy. The device classifies `inst` against its current id, its history of deleted ids and the `lost` flag (table above) and sends the missing ops, a snapshot, a `restore` or a `drop`. Same instance and `seq` equal: nothing to do. |
| `{"type":"ack","list":"config","mid":102}` | Acknowledges the message published with that `mid` (`inst` optional, must match if present). Moves the known cloud position forward to the seq that message carried, never backwards; never triggers a resend. |
| `{"type":"request","list":"config","inst":"a1","from":34,"to":45}` | The cloud found a gap and asks for a range. Handled like a `status` with `seq` = `from - 1`: the device replays from `from` to its latest entry, or sends a snapshot when its oplog no longer reaches back to `from` (rolled over) or `from` is beyond what it ever issued. Throttled by `minCloudRequestIntervalMs`. |
| `{"type":"snapshot","list":"config","inst":"a1","seq":13,"mid":104,"i":0,"last":true,"docs":[...],"crc":...,"count":...}` | Answer to `restore`, same shape as above, every chunk carrying the `mid` of the `restore`. Chunks must arrive in order starting at `0`; `inst` is read from chunk `0`. The last chunk must carry `crc` and `count` computed as above; the device compares them with what it staged and installs the copy only on a match. Documents are compacted by the device before hashing, so the cloud's wire formatting does not matter, but the documents must be the compact strings it received from the device (key order and number formatting are part of the checksum). |

For any issue please reach out to xxxxxxxxxxxxxxxxxxx or raise an issue
