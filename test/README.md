# QuarkDB host unit tests

Runs QuarkDB's real `src/` code on a PC (Linux / WSL / macOS) with GoogleTest.
No board, SPIFFS or Arduino core is needed: the hardware APIs are swapped for
small in-memory mocks at include time. The library code itself is unchanged.

## Running

```bash
cd test
./run_tests.sh            # build + run for both ESP8266 and ESP32 code paths
./run_tests.sh --cloud    # only the cloud backup tests (see "Cloud backup tests")
./run_tests.sh --gtest_filter='QuarkDBTest.Update*'   # any gtest flag works
./run_tests.sh --clean    # wipe the cached build first
```

The only requirement is Docker. Each run of `run_tests.sh`:

1. Builds the `quarkdb-tests` image from `docker/Dockerfile` (Ubuntu 22.04,
   CMake, g++, GoogleTest 1.15.2, ArduinoJson 7.3.0). Docker caches the image,
   so this is only slow the first time or after `docker/` changes.
2. Starts a throwaway container with the repository mounted read-only at
   `/quarkdb` and runs `docker/entrypoint.sh`, which configures, builds and
   runs both test binaries. Tests need no network access.
3. Keeps build output in the `quarkdb-test-build` Docker volume, so later runs
   are incremental and nothing is written into the repository.

### Memory checking (always on)

Every run is built and executed under **AddressSanitizer**, **LeakSanitizer**
and **UndefinedBehaviorSanitizer**. You can't switch them off. The run fails
on the first:

- out-of-bounds read or write, use-after-free, or stack-use-after-return
- memory leak not listed in `docker/lsan.supp`
- undefined behaviour, such as signed overflow, a bad shift or a misaligned
  pointer (`-fno-sanitize-recover`, so these are hard errors, not warnings)

`docker/lsan.supp` suppresses only the library's known one-time allocations:
the helpers created with `new` in `QuarkDB::init()` and in the
`DBReadProcessor`, `DBWriteProcessor`, `DBFilterProcessor` and
`CommandParser` constructors. These classes have no destructors. That's
harmless for a global `QuarkDB` on a device, but remove each entry once its
class frees its helpers.

The script exits non-zero if any test fails, so it can be used as-is in CI.
To use a different image or volume name, set `QUARKDB_TEST_IMAGE` or
`QUARKDB_TEST_VOLUME`.

## How the mocking works

`test/mocks` is first on the include path, so the library's own
`#include <Arduino.h>`, `<FS.h>`, `<SPIFFS.h>` and `<SPI.h>` resolve to:

| Mock | Provides |
|------|----------|
| `mocks/Arduino.h` | `String` (Arduino semantics on top of `std::string`), `Print`, `Printable`, `Stream`, `byte`, `delay`, `millis`, and a `HardwareSerial Serial` double |
| `mocks/FS.h` | `fs::File`, `fs::Dir` and an in-memory `SPIFFS` (`exists/open/remove/openDir`, ESP32 `open("/")` + `openNextFile`) |
| `mocks/SPIFFS.h`, `mocks/SPI.h` | Thin shims so the ESP32 includes compile |
| `mocks/WString.h`, `mocks/Print.h`, `mocks/Stream.h` | Forward to `Arduino.h` for third-party headers that include the core headers directly |
| `mocks/PubSubClient.h` | In-memory double of [PubSubClient](https://github.com/knolleary/pubsubclient) 2.8 with the same public API: records publishes, enforces the real buffer-size rule, tracks subscriptions and connection state, and delivers injected messages to the callback |
| `mocks/Client.h`, `mocks/WiFiClient.h` | Inert `Client`/`WiFiClient` so `PubSubClient mqtt(wifiClient)` compiles |
| `mocks/mock_globals.cpp` | The single `Serial` and `SPIFFS` instances, plus `ESP.random()` / `esp_random()` for list instance ids |

ArduinoJson is built with `ARDUINOJSON_ENABLE_ARDUINO_STRING/PRINT/STREAM=1`
so it works with the mock `String`/`Print` exactly as it does on a device.

The same sources are compiled twice, as `quarkdb_tests_esp8266` (`-DESP8266`)
and `quarkdb_tests_esp32` (`-DESP32`), so both `#if` branches (for example
`getListNames()`) get tested.

### Test hooks

```cpp
SPIFFS.reset();                       // empty FS, clear fault injection
SPIFFS.writeRaw(path, "...");         // seed a list file directly
SPIFFS.readRaw(path);                 // inspect what the DB wrote
SPIFFS.failOpen("w");                 // make open(..., "w") fail ("r"/"a" too)
SPIFFS.setBeginResult(false);         // simulate a mount failure
SPIFFS.powerLossAfterOpen("w");       // cut power right after the next "w" open
SPIFFS.powerLossOnRemove(true);       // cut power mid-remove; file survives half-erased
SPIFFS.powerOn();                     // "reboot": flash keeps what was persisted
SPIFFS.rename(from, to);              // SPIFFS rules: fails if `to` exists
Serial.feed("QUARKDB>db.x.count()\n"); // drive processSerialCommand()
Serial.output();                      // everything the library printed
advanceMillis(ms);                    // move the mock clock (query timeout tests)

mqtt.connect("id") / mqtt.disconnect() / mqtt.setConnected(bool);   // broker connection state
mqtt.setBufferSize(n);                // publish() fails when header + topic + payload exceed it
mqtt.published();                     // every {topic, payload, retained} the device published
mqtt.subscriptions(); mqtt.isSubscribed(topic);
mqtt.failNextPublishes(n); mqtt.failSubscribe(true);   // fault injection
mqtt.deliver(topic, payload);         // hand a cloud message to the callback now
mqtt.queueIncoming(topic, payload);   // ... or on the next mqtt.loop()
```

`unit/test_helpers.h` has the `QuarkDBTest` fixture: a fresh FS and an
initialised `QuarkDB db` for every test, plus `add()`, `update()`,
`seedPeople()` and `findNames()` helpers. `unit/cloud_test_helpers.h` adds
the `CloudSyncTest` fixture used by the cloud backup tests: a mocked
`PubSubClient` wired to the DB through `QuarkMqttTransport`, a
`QuarkSyncConfig` with a topic pair, and helpers to read what the device
published (`message(i)`, `types()`, `mid(i)`), to play a correlating cloud
(`cloudSends()`; `statusJson()` and `snapshotJson()` echo the `mid` of the
latest query/restore the device published for that list, `ackJson()` names
a published `mid`, `snapshotJson()` adds the `crc`/`count` of everything
sent since chunk 0 to the last chunk, with overrides for negative tests), to inspect the sync state (`inst()`,
`seq()`, `state()`, `pending()`, `oplogLines()`) and to `reboot()`.

## Layout

| File | Covers |
|------|--------|
| `unit/quarkdb_crud_test.cpp` | Public API: init, create/delete list, add, count, find (all operators, dot paths, `$eleMatch`, limits), update, delete, FS failure paths, and an end-to-end flow |
| `unit/filter_processor_test.cpp` | `DBFilterProcessor` matching rules in isolation |
| `unit/command_parser_test.cpp` | The `QUARKDB>` serial CLI |
| `unit/db_utils_test.cpp` | List-name and digit validation |
| `unit/mock_fs_test.cpp` | Checks that the mock filesystem behaves correctly |
| `unit/power_loss_test.cpp` | Crash safety: power cut while writing `.tmp`, before or during the remove of `.jsonl` (including a half-erased file), during recovery, and mid-append, plus recovery on every operation |
| `unit/cloud_mqtt_mock_test.cpp` | Checks that the PubSubClient mock behaves like the real client where the library depends on it |
| `unit/cloud_transport_test.cpp` | `QuarkMqttTransport` over the mock, payload limit from the client buffer, MQTT topic filter matching |
| `unit/cloud_oplog_test.cpp` | Per list sync enable/disable, oplog entries for every write path (API and CLI), sequence numbers, persistence across reboots and power loss, rollover at `maxOplogBytes`, tombstones of deleted lists |
| `unit/cloud_sync_test.cpp` | The protocol: `syncCloud()` query, `status` handling (in sync, behind, no copy, ahead, instance mismatch with both restore policies), delta push in order, offline accumulation and flush, failed publish retry, acks, cloud range requests (`request` with `from`/`to`, replayed or answered with a snapshot when the oplog rolled over), waiting for the status answer with re-query after `queryTimeoutMs`, automatic restore of a wiped device on instance mismatch, the history of deleted instances (never restored, bounded, persisted) and the `lost` flag (restored or pushed as snapshot, never deltas), message-id correlation (every publish has a unique `mid`; `status` must echo the open query's, `snapshot` chunks the restore's, `ack` names the op it confirms and acks apply in any order), and the rogue-cloud defences (unsolicited status/snapshots ignored, one answer per query, request throttling, inbound size cap, restore byte cap and timeout, zero publishes for a stream of junk), and the snapshot integrity check (CRC-32 + count on the last chunk, verified against a zlib vector; wrong, missing or miscounted checksums discard the staged copy, outbound snapshots carry them too), chunked snapshots both ways, `restoreFromCloud()`, deleted lists, multiple lists, `getSyncInfo()` and an end-to-end device lifecycle |

## Cloud backup tests

`./run_tests.sh --cloud` runs only the suites named `Cloud*`
(`CloudMqttMockTest`, `CloudTransportTest`, `CloudTopicMatch`,
`CloudOplogTest`, `CloudSyncTest`), on both platforms, with the same
sanitizers. It is the same binary as the full run with
`--gtest_filter=Cloud*` applied, so an explicit `--gtest_filter` takes
precedence. The plain `./run_tests.sh` includes them.

Nothing talks to a broker: `QuarkMqttTransport` publishes into the
`PubSubClient` mock and the tests read `mqtt.published()`. The cloud is
played by the test through `cloudSends(json)`, which delivers the message
on the subscribe topic exactly like `PubSubClient::loop()` would, through
the callback registered with `setCallback()` and `QuarkDB::handleMessage()`.
The message formats are documented in `API.md` ("Cloud Backup over MQTT")
and in `src/sync/sync_manager.h`.

## Behaviour notes

- **Crash-safe rewrites.** `updateRecords()` and `deleteRecords()` write
  the new list to `/__quarkdb__/<list>.tmp`, rename it to `.new` once it is
  complete, remove the `.jsonl`, then rename `.new` to `.jsonl`. SPIFFS
  `rename()` fails if the target exists, so the remove has to come first.
  Every list operation first calls `DBUtils::recoverList()`: a `.tmp` is
  always incomplete and is discarded, and a `.new` is always complete and is
  installed. The decision never depends on whether a remove finished.
- **List names** are 1–12 characters of letters, digits, `_` or `-`. The
  longest path, `/__quarkdb__/<12 chars>.jsonl`, is 31 characters, the SPIFFS
  limit on both cores (`SPIFFS_OBJ_NAME_LEN` 32 including the NUL). The mock
  FS enforces the same 31-character limit.
- **Torn appends.** `addRecord()` adds a newline first if the file doesn't
  end with one, so a partial line left by a power cut can't swallow the next
  record. Partial lines are skipped by reads and counts, and dropped by the
  next rewrite.
- **Error codes.** `getRecords()`, `getRecordCount()`, `updateRecords()` and
  `deleteRecords()` all return `-1` for a missing or invalid list, a bad
  selector, or a filesystem error.
- **TBD stubs.** `removeRecords()` and `removeAllLists()` return `false` and
  `getDBStat()` returns `nullptr` until they're implemented. The processor
  `setMaxRecords()`/`setMaxRecordSize()` store the value but don't enforce it
  yet. The test build doesn't use `--gc-sections`, so any declared-but-undefined
  method that gets called fails the build.
- **Update replaces.** `updateRecords()` replaces the whole document instead
  of merging fields. The tests treat this as intended behaviour.
- **Sync files.** A synced list has `/__quarkdb__/<list>.sync` (instance
  id, sequence counter, options) and `/__quarkdb__/<list>.oplog` next to its
  `.jsonl`; a restore stages the cloud copy in `<list>.rst` and installs it
  through `.new`. None of them show up in `show lists`. The sequence
  counter in `.sync` is only a lower bound (refreshed every 32 changes); the
  newest oplog line carries the real value and is reconciled on load, so
  the counter never goes backwards. `deleteList()` on a
  synced list leaves a tombstone `.sync` (no instance id, the deleted one
  appended to `hist`) until the cloud has been told; `createList()` while
  an instance is live sets `lost`.
- **Sync is observable, not blocking.** Every publish goes through
  `QuarkTransport::publish()` synchronously, so after a write or a delivered
  cloud message the tests can assert on `mqtt.published()` right away.
  Nothing waits for the broker: `restoreFromCloud()` only sends the request
  and the list changes when the last snapshot chunk is handled.
- **Not fixed yet:** the `CommandParser` constructor assigns `fileMode` and
  `chipSelect` to themselves, and the classes never free the helpers they
  create with `new` (see `docker/lsan.supp`).
