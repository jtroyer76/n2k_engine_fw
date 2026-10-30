#ifndef N2K_ENGINE_FW_SRC_TEMPERATURE_LOGIC_H_
#define N2K_ENGINE_FW_SRC_TEMPERATURE_LOGIC_H_

// Pure logic shared by the firmware and the host tests. No Arduino or SensESP
// includes here.

namespace engine_temp {

constexpr double kKelvinOffset = 273.15;

inline double CelsiusToKelvin(double celsius) { return celsius + kKelvinOffset; }
inline double KelvinToCelsius(double kelvin) { return kelvin - kKelvinOffset; }

// A DS18B20 reads exactly +85.0 °C from its power-on scratchpad if it is read
// before a conversion has finished (for example after a brown-out of the
// probe). Drop that value when it is the first reading from a probe, or when it
// appears as a jump from a reading far from 85 °C. A real 85 °C reached
// gradually still gets through.
class PowerOnFilter {
 public:
  // Returns true if the reading should be passed on.
  bool Accept(double celsius) {
    const bool is_power_on_value = celsius == 85.0;
    if (is_power_on_value &&
        (!have_last_ || Abs(last_celsius_ - 85.0) > kMaxJumpCelsius)) {
      return false;
    }
    last_celsius_ = celsius;
    have_last_ = true;
    return true;
  }

 private:
  static constexpr double kMaxJumpCelsius = 10.0;
  static double Abs(double v) { return v < 0 ? -v : v; }
  bool have_last_ = false;
  double last_celsius_ = 0;
};

// High-temperature alarm with hysteresis. A threshold of zero or below means
// the alarm is disabled and never active.
class HighAlarm {
 public:
  HighAlarm(double threshold_celsius = 0, double hysteresis_celsius = 2)
      : threshold_{threshold_celsius}, hysteresis_{hysteresis_celsius} {}

  void Configure(double threshold_celsius, double hysteresis_celsius) {
    threshold_ = threshold_celsius;
    hysteresis_ = hysteresis_celsius < 0 ? 0 : hysteresis_celsius;
    if (!enabled()) active_ = false;
  }

  bool enabled() const { return threshold_ > 0; }
  bool active() const { return active_; }

  // Feed a new reading; returns the alarm state after it.
  bool Update(double celsius) {
    if (!enabled()) {
      active_ = false;
    } else if (!active_ && celsius >= threshold_) {
      active_ = true;
    } else if (active_ && celsius < threshold_ - hysteresis_) {
      active_ = false;
    }
    return active_;
  }

  // No current reading (probe missing or stale): the alarm clears, since
  // there is no data to support it. The display shows the value as missing.
  void Clear() { active_ = false; }

 private:
  double threshold_;
  double hysteresis_;
  bool active_ = false;
};

}  // namespace engine_temp

#endif  // N2K_ENGINE_FW_SRC_TEMPERATURE_LOGIC_H_
