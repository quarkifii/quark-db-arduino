


# QuarkDB
## NoSQL Database for Arduino Based Boards
### Use API or CLI like any DB
An [Arduino](https://arduino.cc/) library for managing data just like any other NoSQL DB.     
Data managed using ArduinoJson and SPIFFS file system for ESP boards.    
API and usage details can be found [here](/API.md)

### Cloud backup over MQTT
Lists can be backed up to the cloud through an MQTT topic pair. The device
stays the master copy: every change is logged to a per list oplog and
published as a delta when connected, accumulated on flash while offline and
sent as a snapshot when the oplog rolled over. A list can be restored from
the cloud copy on demand or automatically after a flash wipe. Works with
[PubSubClient](https://github.com/knolleary/pubsubclient) out of the box
(`QuarkMqttTransport.h`) or any client behind the `QuarkTransport`
interface (`QuarkTransport.h`). See [Cloud Backup over MQTT](/API.md#cloud-backup-over-mqtt) and
[examples/cloud-sync](examples/cloud-sync).


## Compatible Hardware

* ESP8266 based boards
* ESP32 based boards    
  *NOTE:* Other boards will be supported soon.

## Installation

### Using the Arduino IDE Library Manager

1. Choose `Sketch` -> `Include Library` -> `Manage Libraries...`
2. Type `QuarkDB` into the search box.
3. Click the row to select the library.
4. Click the `Install` button to install the library.

### Using Git
``` 
cd ~/Documents/Arduino/libraries/ 
git clone https://github.com/quarkifii/quark-db-arduino QuarkDB 
``` 
## API and CLI

See [API and CLI commands](/API.md).

## Examples

See [examples](examples) folder.

## Tests

Host unit tests (no board needed) live in [test](test); `./run_tests.sh`
runs everything and `./run_tests.sh --cloud` runs the cloud backup tests.

## More Information

More information of Quarkifi DB can be found [here](https://qstream.quarkifi.com/quarkdb/).

For any issue please reach out to xxxxxxxxxxxxxxxxxxx or raise an issue.


## License

This libary is [licensed](LICENSE) under the [MIT Licence](https://en.wikipedia.org/wiki/MIT_License).