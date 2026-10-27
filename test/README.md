# QuarkDB host unit tests

Runs QuarkDB's real `src/` code on a PC (Linux / WSL / macOS) with GoogleTest.
No board, SPIFFS or Arduino core is needed: the hardware APIs are swapped for
small in-memory mocks at include time. The library code itself is unchanged.

## Running

```bash
cd test
./run_tests.sh            # build + run for both ESP8266 and ESP32 code paths
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
| `mocks/mock_globals.cpp` | The single `Serial` and `SPIFFS` instances |

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
SPIFFS.powerOn();                     // "reboot": flash keeps what was persisted
SPIFFS.rename(from, to);              // SPIFFS rules: fails if `to` exists
Serial.feed("QUARKDB>db.x.count()\n"); // drive processSerialCommand()
Serial.output();                      // everything the library printed
```

`unit/test_helpers.h` has the `QuarkDBTest` fixture: a fresh FS and an
initialised `QuarkDB db` for every test, plus `add()`, `update()`,
`seedPeople()` and `findNames()` helpers.

## Layout

| File | Covers |
|------|--------|
| `unit/quarkdb_crud_test.cpp` | Public API: init, create/delete list, add, count, find (all operators, dot paths, `$eleMatch`, limits), update, delete, FS failure paths, and an end-to-end flow |
| `unit/filter_processor_test.cpp` | `DBFilterProcessor` matching rules in isolation |
| `unit/command_parser_test.cpp` | The `QUARKDB>` serial CLI |
| `unit/db_utils_test.cpp` | List-name and digit validation |
| `unit/mock_fs_test.cpp` | Checks that the mock filesystem behaves correctly |
| `unit/power_loss_test.cpp` | Crash safety: power cut while writing the temp file, between remove and rename, and mid-append, plus recovery on every operation |

## Behaviour notes

- **Crash-safe rewrites.** `updateRecords()` and `deleteRecords()` write the
  new list to `/__quarkdb__/<list>.tmp`, then remove the `.jsonl` and rename
  the temp file into place. SPIFFS `rename()` fails if the target exists,
  so the remove has to come first. Every list operation first calls
  `DBUtils::recoverList()`, which discards a temp file if the `.jsonl` is
  still there, or completes the rename if it isn't.
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
- **Not fixed yet:** the `CommandParser` constructor assigns `fileMode` and
  `chipSelect` to themselves, and the classes never free the helpers they
  create with `new` (see `docker/lsan.supp`).
