#ifndef SENSOR_CALIBRATION_HPP
#define SENSOR_CALIBRATION_HPP

#include <Arduino.h>
#include <Preferences.h>

namespace SensorCalibration {

enum SensorId : uint8_t {
  Bean = 0,
  Fan = 1
};

struct Point {
  double rawTempF;
  double referenceTempF;
};

struct Fit {
  bool enabled;
  double slope;
  double offset;
  double rmse;
  uint8_t pointCount;
};

class Controller {
public:
  void load(Preferences &preferences) {
    loadFit(preferences, Bean, beanFit);
    loadFit(preferences, Fan, fanFit);
  }

  double correct(SensorId sensor, double rawTempF) const {
    const Fit &fit = getFit(sensor);
    return fit.enabled ? fit.slope * rawTempF + fit.offset : rawTempF;
  }

  const Fit &getFit(SensorId sensor) const {
    return sensor == Fan ? fanFit : beanFit;
  }

  bool apply(SensorId sensor,
             const Point *points,
             uint8_t pointCount,
             Preferences &preferences,
             String &errorMessage) {
    Fit nextFit;
    if (!calculateFit(points, pointCount, nextFit, errorMessage)) {
      return false;
    }

    saveFit(preferences, sensor, nextFit);
    if (sensor == Fan) {
      fanFit = nextFit;
    } else {
      beanFit = nextFit;
    }
    return true;
  }

  void reset(SensorId sensor, Preferences &preferences) {
    Fit identity = {false, 1.0, 0.0, 0.0, 0};
    saveFit(preferences, sensor, identity);
    if (sensor == Fan) {
      fanFit = identity;
    } else {
      beanFit = identity;
    }
  }

  static bool fit(const Point *points,
                  uint8_t pointCount,
                  Fit &result,
                  String &errorMessage) {
    return calculateFit(points, pointCount, result, errorMessage);
  }

private:
  static constexpr uint8_t MIN_POINT_COUNT = 3;
  static constexpr double MIN_RAW_SPAN_F = 50.0;
  static constexpr double MIN_SLOPE = 0.80;
  static constexpr double MAX_SLOPE = 1.20;
  static constexpr double MAX_OFFSET_F = 60.0;
  static constexpr double MAX_CORRECTION_F = 60.0;
  static constexpr double MIN_VALID_TEMP_F = 0.0;
  static constexpr double MAX_VALID_TEMP_F = 600.0;

  Fit beanFit = {false, 1.0, 0.0, 0.0, 0};
  Fit fanFit = {false, 1.0, 0.0, 0.0, 0};

  static const char *key(SensorId sensor, const char *suffix) {
    return sensor == Fan ? (strcmp(suffix, "slope") == 0 ? "fan_cal_slope" :
                            strcmp(suffix, "offset") == 0 ? "fan_cal_offset" :
                            strcmp(suffix, "rmse") == 0 ? "fan_cal_rmse" :
                            strcmp(suffix, "points") == 0 ? "fan_cal_points" : "fan_cal_enabled")
                         : (strcmp(suffix, "slope") == 0 ? "bean_cal_slope" :
                            strcmp(suffix, "offset") == 0 ? "bean_cal_offset" :
                            strcmp(suffix, "rmse") == 0 ? "bean_cal_rmse" :
                            strcmp(suffix, "points") == 0 ? "bean_cal_points" : "bean_cal_enabled");
  }

  void loadFit(Preferences &preferences, SensorId sensor, Fit &fit) {
    fit.enabled = preferences.getBool(key(sensor, "enabled"), false);
    fit.slope = preferences.getDouble(key(sensor, "slope"), 1.0);
    fit.offset = preferences.getDouble(key(sensor, "offset"), 0.0);
    fit.rmse = preferences.getDouble(key(sensor, "rmse"), 0.0);
    fit.pointCount = preferences.getUChar(key(sensor, "points"), 0);

    if (fit.slope < MIN_SLOPE || fit.slope > MAX_SLOPE ||
        fabs(fit.offset) > MAX_OFFSET_F || fit.pointCount > 8) {
      fit = {false, 1.0, 0.0, 0.0, 0};
    }
  }

  static void saveFit(Preferences &preferences, SensorId sensor, const Fit &fit) {
    preferences.putBool(key(sensor, "enabled"), fit.enabled);
    preferences.putDouble(key(sensor, "slope"), fit.slope);
    preferences.putDouble(key(sensor, "offset"), fit.offset);
    preferences.putDouble(key(sensor, "rmse"), fit.rmse);
    preferences.putUChar(key(sensor, "points"), fit.pointCount);
  }

  static bool calculateFit(const Point *points,
                           uint8_t pointCount,
                           Fit &fit,
                           String &errorMessage) {
    if (points == nullptr || pointCount < MIN_POINT_COUNT || pointCount > 8) {
      errorMessage = "Enter at least 3 calibration points.";
      return false;
    }

    double rawMin = points[0].rawTempF;
    double rawMax = points[0].rawTempF;
    double rawSum = 0.0;
    double referenceSum = 0.0;
    for (uint8_t index = 0; index < pointCount; index++) {
      const Point &point = points[index];
      if (!isfinite(point.rawTempF) || !isfinite(point.referenceTempF) ||
          point.rawTempF < MIN_VALID_TEMP_F || point.rawTempF > MAX_VALID_TEMP_F ||
          point.referenceTempF < MIN_VALID_TEMP_F || point.referenceTempF > MAX_VALID_TEMP_F) {
        errorMessage = "Calibration temperatures must be between 0F and 600F.";
        return false;
      }
      rawMin = min(rawMin, point.rawTempF);
      rawMax = max(rawMax, point.rawTempF);
      rawSum += point.rawTempF;
      referenceSum += point.referenceTempF;
    }

    if (rawMax - rawMin < MIN_RAW_SPAN_F) {
      errorMessage = "Use setpoints spanning at least 50F.";
      return false;
    }

    const double rawMean = rawSum / pointCount;
    const double referenceMean = referenceSum / pointCount;
    double denominator = 0.0;
    double numerator = 0.0;
    for (uint8_t index = 0; index < pointCount; index++) {
      const double rawDelta = points[index].rawTempF - rawMean;
      denominator += rawDelta * rawDelta;
      numerator += rawDelta * (points[index].referenceTempF - referenceMean);
    }

    if (denominator < 0.0001) {
      errorMessage = "Calibration points must use different temperatures.";
      return false;
    }

    fit.slope = numerator / denominator;
    fit.offset = referenceMean - fit.slope * rawMean;
    if (fit.slope < MIN_SLOPE || fit.slope > MAX_SLOPE || fabs(fit.offset) > MAX_OFFSET_F) {
      errorMessage = "The fitted correction is outside the allowed range.";
      return false;
    }

    double squaredError = 0.0;
    for (uint8_t index = 0; index < pointCount; index++) {
      const double corrected = fit.slope * points[index].rawTempF + fit.offset;
      if (fabs(corrected - points[index].rawTempF) > MAX_CORRECTION_F) {
        errorMessage = "The correction is too large; check probe placement and readings.";
        return false;
      }
      const double residual = corrected - points[index].referenceTempF;
      squaredError += residual * residual;
    }

    fit.enabled = true;
    fit.rmse = sqrt(squaredError / pointCount);
    fit.pointCount = pointCount;
    return true;
  }
};

} // namespace SensorCalibration

#endif