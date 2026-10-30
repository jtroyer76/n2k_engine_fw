// n2k_engine_fw: engine temperatures from 1-Wire probes to NMEA 2000, on the
// Hat Labs HALMET.
//
// Based on the HALMET example firmware, Copyright Hat Labs Ltd, Apache-2.0
// (https://github.com/hatlabs/HALMET-example-firmware).

#include <Adafruit_SSD1306.h>

#include <memory>

#include "alarm_config.h"
#include "counting_nmea2000.h"
#include "halmet_const.h"
#include "halmet_display.h"
#include "halmet_serial.h"
#include "n2k_engine_senders.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/linear.h"
#include "sensesp/ui/config_item.h"
#include "sensesp/ui/status_page_item.h"
#include "sensesp/ui/ui_controls.h"
#include "sensesp_app_builder.h"
#include "sensesp_onewire/onewire_temperature.h"
#include "temperature_logic.h"

// Optional, gitignored. Define OTA_PASSWORD there to enable updates over WiFi.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

using namespace sensesp;
using namespace sensesp::onewire;
using namespace halmet;
using namespace engine_temp;

namespace {

const char* kFirmwareVersion = "0.1.0";

// HALMET 1-Wire data pin "DQ".
constexpr int kOneWirePin = 4;
// How often each probe is read. A 12-bit DS18B20 conversion takes up to
// 750 ms; the library enforces the minimum.
constexpr uint32_t kProbeReadIntervalMs = 1000;
// A probe with no good reading for this long counts as missing.
constexpr uint32_t kProbeStaleMs = 10000;

/// One temperature probe and everything derived from it.
struct Probe {
  String name;
  PowerOnFilter filter;
  HighAlarm alarm;
  elapsedMillis age;
  bool fresh = false;
  float celsius = 0;
  // Calibrated readings in kelvin.
  ObservableValue<float> kelvin;
  std::shared_ptr<StatusPageItem<String>> status;
};

Probe* SetupProbe(DallasTemperatureSensors* dts, const char* key,
                  const char* name, const char* description, int sort_order) {
  auto* probe = new Probe();
  probe->name = name;
  String base = String("/Probes/") + key;

  auto* sensor = new OneWireTemperature(dts, kProbeReadIntervalMs,
                                        base + "/Address");
  ConfigItem(sensor)
      ->set_title(String(name) + " probe")
      ->set_description(description)
      ->set_sort_order(sort_order);

  // Offset in °C (a difference in °C equals one in kelvin).
  auto* calibration = new Linear(1.0, 0.0, base + "/Calibration");
  ConfigItem(calibration)
      ->set_title(String(name) + " calibration")
      ->set_description(
          "Multiplier and offset applied to the reading. Offset in °C.")
      ->set_sort_order(sort_order + 1);

  auto* alarm_config = new AlarmConfig(base + "/Alarm", &probe->alarm);
  ConfigItem(alarm_config)
      ->set_title(String(name) + " alarm")
      ->set_sort_order(sort_order + 2);

  // The power-on filter looks at the raw value, before calibration.
  sensor->connect_to(new LambdaConsumer<float>([probe, calibration](float k) {
    if (probe->filter.Accept(KelvinToCelsius(k))) {
      calibration->set(k);
    }
  }));
  calibration->connect_to(new LambdaConsumer<float>([probe](float k) {
    probe->celsius = KelvinToCelsius(k);
    probe->age = 0;
    probe->fresh = true;
    probe->alarm.Update(probe->celsius);
    probe->kelvin.set(k);
  }));

  probe->status = std::make_shared<StatusPageItem<String>>(
      name, "no reading", "Temperatures", sort_order);
  return probe;
}

String FormatProbe(const Probe* probe) {
  if (!probe->fresh) {
    return "no reading";
  }
  String s = String(probe->celsius, 1) + " °C";
  if (probe->alarm.active()) {
    s += " ALARM";
  }
  return s;
}

/// Digital alarm input; the returned flag follows the input when enabled.
bool* SetupDigitalAlarm(int pin, const char* key, const char* name,
                        int sort_order) {
  auto* state = new bool(false);
  auto* config = new DigitalAlarmConfig(String("/Inputs/") + key);
  ConfigItem(config)
      ->set_title(name)
      ->set_description(
          "Engine panel warning line. Off by default; measure the line's "
          "voltage with the light on and off before connecting.")
      ->set_sort_order(sort_order);
  if (config->enabled()) {
    auto* input = new DigitalInputState(pin, INPUT, 100);
    bool invert = config->invert();
    input->connect_to(new LambdaConsumer<bool>(
        [state, invert](bool value) { *state = invert ? !value : value; }));
  }
  return state;
}

}  // namespace

CountingNMEA2000* nmea2000;
Adafruit_SSD1306* display;

void setup() {
  SetupLogging(ESP_LOG_DEBUG);
  Serial.begin(115200);

  /////////////////////////////////////////////////////////////////////
  // Application framework: WiFi, web UI, OTA. Data never depends on WiFi.

  SensESPAppBuilder builder;
  builder.set_hostname("engine-temps");
#ifdef OTA_PASSWORD
  builder.enable_ota(OTA_PASSWORD);
#endif
  sensesp_app = builder.get_app();

  auto i2c = new TwoWire(0);
  i2c->begin(kSDAPin, kSCLPin);

  /////////////////////////////////////////////////////////////////////
  // NMEA 2000

  nmea2000 = new CountingNMEA2000(kCANTxPin, kCANRxPin);
  // Kept small, as in the HALMET example, to leave contiguous heap for TLS.
  nmea2000->SetN2kCANSendFrameBufSize(96);
  nmea2000->SetProductInformation(
      "n2k_engine_fw",   // Model serial code
      104,               // Product code (from the HALMET example)
      "HALMET engine temperatures",  // Model ID
      kFirmwareVersion,  // Software version
      "HALMET"           // Model version
  );
  nmea2000->SetDeviceInformation(GetBoardSerialNumber(),
                                 140,    // Device function: Engine
                                 50,     // Device class: Propulsion
                                 2046);  // Manufacturer code (HALMET example)
  nmea2000->SetMode(tNMEA2000::N2km_NodeOnly, 71);
  nmea2000->EnableForward(false);
  nmea2000->Open();
  event_loop()->onRepeat(1, []() { nmea2000->ParseMessages(); });

  auto n2k_tx_status = std::make_shared<StatusPageItem<int>>(
      "NMEA 2000 transmitted messages", 0, "NMEA 2000", 310);
  nmea2000->tx_count_.connect_to(n2k_tx_status);

  auto* engine_sender = new N2kEngineTemperatureSender(
      "/NMEA 2000/Engine", 0, nmea2000);
  ConfigItem(engine_sender)
      ->set_title("Engine (PGN 127489)")
      ->set_description(
          "Coolant temperature and alarm status bits are sent as Engine "
          "Parameters, Dynamic.")
      ->set_sort_order(1000);

  auto* exhaust_sender = new N2kTemperatureExtSender(
      "/NMEA 2000/Exhaust", 0, N2kts_ExhaustGasTemperature, nmea2000);
  ConfigItem(exhaust_sender)
      ->set_title("Exhaust (PGN 130316)")
      ->set_sort_order(1010);

  auto* engine_room_sender = new N2kTemperatureExtSender(
      "/NMEA 2000/Engine Room", 0, N2kts_EngineRoomTemperature, nmea2000);
  ConfigItem(engine_room_sender)
      ->set_title("Engine room (PGN 130316)")
      ->set_description(
          "Source 3 (Engine Room) by default. If your display doesn't show "
          "it, try source 2 (Inside).")
      ->set_sort_order(1020);

  /////////////////////////////////////////////////////////////////////
  // 1-Wire probes. A role with no configured address takes the next probe
  // found on the bus; set the addresses in the web UI.

  auto* dts = new DallasTemperatureSensors(kOneWirePin);

  Probe* coolant = SetupProbe(dts, "Coolant", "Coolant",
                              "Clamped to the thermostat housing or coolant "
                              "outlet.",
                              100);
  Probe* exhaust = SetupProbe(dts, "Exhaust", "Exhaust",
                              "Wet exhaust hose, just downstream of the "
                              "mixing elbow.",
                              200);
  Probe* engine_room = SetupProbe(dts, "Engine Room", "Engine room",
                                  "Engine compartment air, away from hot "
                                  "surfaces.",
                                  300);

  coolant->kelvin.connect_to(new LambdaConsumer<float>(
      [engine_sender](float k) { engine_sender->temperature_->set(k); }));
  exhaust->kelvin.connect_to(new LambdaConsumer<float>(
      [exhaust_sender](float k) { exhaust_sender->temperature_->set(k); }));
  engine_room->kelvin.connect_to(new LambdaConsumer<float>(
      [engine_room_sender](float k) {
        engine_room_sender->temperature_->set(k);
      }));

  // Probes on the bus that no role uses, so they can be assigned.
  String unassigned;
  OWDevAddr addr;
  while (dts->get_next_address(&addr)) {
    char addr_str[24];
    snprintf(addr_str, sizeof(addr_str),
             "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1],
             addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
    if (unassigned.length() > 0) unassigned += ", ";
    unassigned += addr_str;
    dts->register_address(addr);
  }
  static auto unassigned_status = std::make_shared<StatusPageItem<String>>(
      "Unassigned probes", unassigned.length() ? unassigned : String("none"),
      "Temperatures", 400);

  /////////////////////////////////////////////////////////////////////
  // Digital alarm inputs from the engine panel. D1 is left for a future
  // tach input.

  bool* oil_pressure_input =
      SetupDigitalAlarm(kDigitalInputPin2, "D2", "D2 oil pressure alarm", 2000);
  bool* temperature_input =
      SetupDigitalAlarm(kDigitalInputPin3, "D3", "D3 temperature alarm", 2010);

  /////////////////////////////////////////////////////////////////////
  // Once a second: expire stale probes, update alarms and status.

  static Probe* probes[] = {coolant, exhaust, engine_room};
  event_loop()->onRepeat(1000, [=]() {
    for (Probe* probe : probes) {
      if (probe->fresh && probe->age > kProbeStaleMs) {
        probe->fresh = false;
        probe->alarm.Clear();
      }
      probe->status->set(FormatProbe(probe));
    }
    engine_sender->over_temperature_ =
        coolant->alarm.active() || *temperature_input;
    // Exhaust overheating means raw-water flow has dropped.
    engine_sender->water_flow_ = exhaust->alarm.active();
    engine_sender->low_oil_pressure_ = *oil_pressure_input;
  });

  /////////////////////////////////////////////////////////////////////
  // Optional OLED display

  if (InitializeSSD1306(sensesp_app, &display, i2c)) {
    event_loop()->onRepeat(1000, [=]() {
      PrintValue(display, 1, "IP:", WiFi.localIP().toString());
      PrintValue(display, 2, "Coolant", FormatProbe(coolant));
      PrintValue(display, 3, "Exhaust", FormatProbe(exhaust));
      PrintValue(display, 4, "Eng room", FormatProbe(engine_room));
    });
  }

  // To avoid garbage collecting all shared pointers created in setup(),
  // loop from here.
  while (true) {
    loop();
  }
}

void loop() { event_loop()->tick(); }
