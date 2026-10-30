#ifndef N2K_ENGINE_FW_SRC_N2K_ENGINE_SENDERS_H_
#define N2K_ENGINE_FW_SRC_N2K_ENGINE_SENDERS_H_

#include <N2kMessages.h>
#include <NMEA2000.h>

#include "counting_nmea2000.h"
#include "sensesp/system/saveable.h"
#include "sensesp/transforms/repeat.h"
#include "sensesp_base_app.h"

namespace engine_temp {

/**
 * @brief Transmit NMEA 2000 PGN 130316: Temperature, Extended Range
 *
 * The input is in kelvin. When no reading has arrived within the expiry time,
 * the temperature is sent as "not available" rather than the last value.
 */
class N2kTemperatureExtSender : public sensesp::FileSystemSaveable {
 public:
  N2kTemperatureExtSender(String config_path, uint8_t instance,
                          tN2kTempSource source, CountingNMEA2000* nmea2000)
      : sensesp::FileSystemSaveable{config_path},
        instance_{instance},
        source_{source},
        nmea2000_{nmea2000},
        repeat_interval_{2000},  // In ms. Standard rate for PGN 130316.
        expiry_{10000}           // In ms. When the input expires.
  {
    this->load();
    temperature_ = std::make_shared<sensesp::RepeatExpiring<double>>(
        repeat_interval_, expiry_);

    sensesp::event_loop()->onRepeat(repeat_interval_, [this]() {
      tN2kMsg N2kMsg;
      SetN2kTemperatureExt(N2kMsg, sid_++, this->instance_, this->source_,
                           this->temperature_->get(), N2kDoubleNA);
      this->nmea2000_->SendMsg(N2kMsg);
    });
  }

  virtual bool from_json(const JsonObject& config) override {
    if (!config["instance"].is<int>() || !config["source"].is<int>()) {
      return false;
    }
    instance_ = config["instance"];
    source_ = static_cast<tN2kTempSource>(config["source"].as<int>());
    return true;
  }

  virtual bool to_json(JsonObject& config) override {
    config["instance"] = instance_;
    config["source"] = static_cast<int>(source_);
    return true;
  }

  // Temperature in kelvin.
  std::shared_ptr<sensesp::RepeatExpiring<double>> temperature_;

 protected:
  uint8_t instance_;
  tN2kTempSource source_;
  CountingNMEA2000* nmea2000_;
  unsigned int repeat_interval_;
  unsigned int expiry_;
  unsigned char sid_ = 0;
};

/**
 * @brief Transmit NMEA 2000 PGN 127489: Engine Parameters, Dynamic
 *
 * Carries only what this device measures: engine (coolant) temperature and
 * the alarm status bits. Every other field is sent as "not available", so
 * displays don't show zero oil pressure or engine hours for an engine whose
 * other data comes from elsewhere.
 */
class N2kEngineTemperatureSender : public sensesp::FileSystemSaveable {
 public:
  N2kEngineTemperatureSender(String config_path, uint8_t engine_instance,
                             CountingNMEA2000* nmea2000)
      : sensesp::FileSystemSaveable{config_path},
        engine_instance_{engine_instance},
        nmea2000_{nmea2000},
        repeat_interval_{500},  // In ms. Standard rate for PGN 127489.
        expiry_{10000}          // In ms. When the temperature input expires.
  {
    this->load();
    temperature_ = std::make_shared<sensesp::RepeatExpiring<double>>(
        repeat_interval_, expiry_);

    sensesp::event_loop()->onRepeat(repeat_interval_, [this]() {
      tN2kEngineDiscreteStatus1 status1 = 0;
      status1.Bits.OverTemperature = over_temperature_;
      status1.Bits.LowOilPressure = low_oil_pressure_;
      status1.Bits.WaterFlow = water_flow_;
      status1.Bits.CheckEngine =
          over_temperature_ || low_oil_pressure_ || water_flow_;
      tN2kEngineDiscreteStatus2 status2 = 0;

      tN2kMsg N2kMsg;
      SetN2kEngineDynamicParam(N2kMsg, this->engine_instance_,
                               N2kDoubleNA,  // oil pressure
                               N2kDoubleNA,  // oil temperature
                               this->temperature_->get(),
                               N2kDoubleNA,  // alternator voltage
                               N2kDoubleNA,  // fuel rate
                               N2kDoubleNA,  // engine hours
                               N2kDoubleNA,  // coolant pressure
                               N2kDoubleNA,  // fuel pressure
                               N2kInt8NA,    // engine load
                               N2kInt8NA,    // engine torque
                               status1, status2);
      this->nmea2000_->SendMsg(N2kMsg);
    });
  }

  virtual bool from_json(const JsonObject& config) override {
    if (!config["engine_instance"].is<int>()) {
      return false;
    }
    engine_instance_ = config["engine_instance"];
    return true;
  }

  virtual bool to_json(JsonObject& config) override {
    config["engine_instance"] = engine_instance_;
    return true;
  }

  // Engine (coolant) temperature in kelvin.
  std::shared_ptr<sensesp::RepeatExpiring<double>> temperature_;

  // Status bits, set by the alarm logic.
  bool over_temperature_ = false;
  bool low_oil_pressure_ = false;
  bool water_flow_ = false;

 protected:
  uint8_t engine_instance_;
  CountingNMEA2000* nmea2000_;
  unsigned int repeat_interval_;
  unsigned int expiry_;
};

inline const String ConfigSchema(const N2kEngineTemperatureSender& obj) {
  return R"###({
    "type": "object",
    "properties": {
      "engine_instance": { "title": "Engine instance", "type": "integer", "description": "NMEA 2000 engine instance (0 = single or port engine, 1 = starboard). Must match the device that sends RPM for this engine." }
    }
  })###";
}

inline const String ConfigSchema(const N2kTemperatureExtSender& obj) {
  return R"###({
    "type": "object",
    "properties": {
      "instance": { "title": "Temperature instance", "type": "integer", "description": "NMEA 2000 temperature instance (0-252)" },
      "source": { "title": "Temperature source", "type": "integer", "description": "NMEA 2000 temperature source: 2 = Inside, 3 = Engine Room, 14 = Exhaust Gas. Some displays show only some sources." }
    }
  })###";
}

}  // namespace engine_temp

#endif  // N2K_ENGINE_FW_SRC_N2K_ENGINE_SENDERS_H_
