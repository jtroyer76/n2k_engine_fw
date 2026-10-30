#ifndef N2K_ENGINE_FW_SRC_ALARM_CONFIG_H_
#define N2K_ENGINE_FW_SRC_ALARM_CONFIG_H_

#include "sensesp/system/saveable.h"
#include "temperature_logic.h"

namespace engine_temp {

/**
 * @brief Web UI settings for one high-temperature alarm.
 *
 * Threshold and hysteresis are in °C. A threshold of 0 disables the alarm,
 * which is the default: thresholds come from a logged baseline, not from
 * guesses.
 */
class AlarmConfig : public sensesp::FileSystemSaveable {
 public:
  AlarmConfig(String config_path, HighAlarm* alarm)
      : sensesp::FileSystemSaveable{config_path}, alarm_{alarm} {
    load();
    alarm_->Configure(threshold_, hysteresis_);
  }

  virtual bool from_json(const JsonObject& config) override {
    if (!config["threshold"].is<float>() || !config["hysteresis"].is<float>()) {
      return false;
    }
    threshold_ = config["threshold"];
    hysteresis_ = config["hysteresis"];
    alarm_->Configure(threshold_, hysteresis_);
    return true;
  }

  virtual bool to_json(JsonObject& config) override {
    config["threshold"] = threshold_;
    config["hysteresis"] = hysteresis_;
    return true;
  }

 private:
  HighAlarm* alarm_;
  float threshold_ = 0;
  float hysteresis_ = 2;
};

inline const String ConfigSchema(const AlarmConfig& obj) {
  return R"###({
    "type": "object",
    "properties": {
      "threshold": { "title": "Alarm threshold (°C)", "type": "number", "description": "Alarm when the reading reaches this value. 0 = off." },
      "hysteresis": { "title": "Hysteresis (°C)", "type": "number", "description": "The alarm clears when the reading falls this far below the threshold." }
    }
  })###";
}

/**
 * @brief Web UI settings for one digital alarm input.
 *
 * Off by default, so an unconnected input can't raise an alarm. "Invert"
 * flips the input: set it so the alarm shows when the engine panel's warning
 * light is on (test with the light on and off).
 */
class DigitalAlarmConfig : public sensesp::FileSystemSaveable {
 public:
  explicit DigitalAlarmConfig(String config_path)
      : sensesp::FileSystemSaveable{config_path} {
    load();
  }

  virtual bool from_json(const JsonObject& config) override {
    if (!config["enabled"].is<bool>() || !config["invert"].is<bool>()) {
      return false;
    }
    enabled_ = config["enabled"];
    invert_ = config["invert"];
    return true;
  }

  virtual bool to_json(JsonObject& config) override {
    config["enabled"] = enabled_;
    config["invert"] = invert_;
    return true;
  }

  bool enabled() const { return enabled_; }
  bool invert() const { return invert_; }

 private:
  bool enabled_ = false;
  bool invert_ = false;
};

inline const String ConfigSchema(const DigitalAlarmConfig& obj) {
  return R"###({
    "type": "object",
    "properties": {
      "enabled": { "title": "Enabled", "type": "boolean" },
      "invert": { "title": "Invert", "type": "boolean", "description": "Flip the input so the alarm matches the panel's warning light." }
    }
  })###";
}

inline bool ConfigRequiresRestart(const DigitalAlarmConfig& obj) { return true; }

}  // namespace engine_temp

#endif  // N2K_ENGINE_FW_SRC_ALARM_CONFIG_H_
