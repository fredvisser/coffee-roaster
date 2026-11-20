#include "../../src/network/PIDTuningJson.hpp"
#include <cassert>

unsigned long testMillis = 1000;

int main() {
    JsonDocument document;
    JsonArray rows = document.to<JsonArray>();
    PIDTuningStore::Record record;
    record.id = 4;
    strlcpy(record.firmware, "first", sizeof(record.firmware));
    strlcpy(record.outcome, "inconclusive", sizeof(record.outcome));
    strlcpy(record.reason, "insufficient_or_changing_data", sizeof(record.reason));
    strlcpy(record.load, "loaded", sizeof(record.load));
    pidRecordJson(rows.add<JsonObject>(), record, false);
    record.id = 1;
    strlcpy(record.firmware, "second", sizeof(record.firmware));
    strlcpy(record.outcome, "cancelled", sizeof(record.outcome));
    strlcpy(record.reason, "fan_sensor_failed", sizeof(record.reason));
    strlcpy(record.load, "empty", sizeof(record.load));
    pidRecordJson(rows.add<JsonObject>(), record, false);
    record = {};
    std::string encoded;
    serializeJson(document, encoded);
    JsonDocument restored;
    assert(!deserializeJson(restored, encoded));
    assert(restored[0]["reason"] == "insufficient_or_changing_data");
    assert(restored[1]["reason"] == "fan_sensor_failed");
    assert(restored[0]["firmware"] == "first" && restored[1]["firmware"] == "second");
    assert(restored[0]["outcome"] == "inconclusive" && restored[1]["outcome"] == "cancelled");
    assert(restored[0]["load"] == "loaded" && restored[1]["load"] == "empty");
    puts("PID history JSON ownership regression passed");
}
