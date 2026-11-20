#ifndef PID_TUNING_JSON_HPP
#define PID_TUNING_JSON_HPP

#include <ArduinoJson.h>
#include "../control/PIDTuningStore.hpp"

static void pidConfigurationJson(JsonObject out, const PIDTuningStore::Configuration &config) {
  out["kp"] = config.kp; out["ki"] = config.ki; out["kd"] = config.kd;
  out["rampScale"] = config.rampScale;
  JsonArray bands = out["bands"].to<JsonArray>();
  for (const auto &band : config.bands) {
    if (!band.valid) continue;
    JsonObject item = bands.add<JsonObject>();
    item["minTemp"] = band.minTemp; item["maxTemp"] = band.maxTemp; item["targetTemp"] = band.target;
    item["kp"] = band.kp; item["ki"] = band.ki; item["kd"] = band.kd;
    item["heaterCoeff"] = band.heater; item["coolingCoeff"] = band.cooling;
    item["drift"] = band.drift; item["deadTime"] = band.delay;
  }
}

static void pidRecordJson(JsonObject out, const PIDTuningStore::Record &record, bool detail) {
  out["id"] = record.id; out["parentId"] = record.parentId; out["bootId"] = record.bootId;
  out["unixTime"] = record.unixTime; out["uptimeMs"] = record.uptimeMs;
  // Const arrays otherwise look like string literals and retain pointers into reused records.
  out["firmware"] = JsonString(record.firmware);
  out["durationSeconds"] = record.durationMs / 1000.0; out["outcome"] = JsonString(record.outcome);
  out["reason"] = JsonString(record.reason); out["fan"] = record.fan; out["load"] = JsonString(record.load);
  out["tauFactor"] = record.tauFactor; out["fitRmse"] = record.fitRmse;
  out["predictionRmse"] = record.predictionRmse; out["noise"] = record.noise;
  out["rampMae"] = record.rampMae; out["holdMae"] = record.holdMae;
  out["rampWithin3Percent"] = record.rampWithin3Percent;
  out["overshoot"] = record.overshoot; out["saturatedPercent"] = record.saturatedPercent;
  out["lagSeconds"] = record.lagSeconds;
  out["kp"] = record.proposed.kp; out["ki"] = record.proposed.ki; out["kd"] = record.proposed.kd;
  if (detail) {
    pidConfigurationJson(out["before"].to<JsonObject>(), record.before);
    pidConfigurationJson(out["proposed"].to<JsonObject>(), record.proposed);
  }
}

#endif
