#include <unity.h>

#include "../../src/temperature_logic.h"

using engine_temp::CelsiusToKelvin;
using engine_temp::HighAlarm;
using engine_temp::KelvinToCelsius;
using engine_temp::PowerOnFilter;

void setUp() {}
void tearDown() {}

void test_kelvin_conversion() {
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 273.15, CelsiusToKelvin(0));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 358.15, CelsiusToKelvin(85));
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, -40.0, KelvinToCelsius(233.15));
}

void test_power_on_value_dropped_as_first_reading() {
  PowerOnFilter f;
  TEST_ASSERT_FALSE(f.Accept(85.0));
  TEST_ASSERT_TRUE(f.Accept(21.5));
}

void test_power_on_value_dropped_as_jump() {
  PowerOnFilter f;
  TEST_ASSERT_TRUE(f.Accept(40.0));
  TEST_ASSERT_FALSE(f.Accept(85.0));
  TEST_ASSERT_TRUE(f.Accept(40.5));
}

void test_real_85_reached_gradually_passes() {
  PowerOnFilter f;
  TEST_ASSERT_TRUE(f.Accept(80.0));
  TEST_ASSERT_TRUE(f.Accept(84.9));
  TEST_ASSERT_TRUE(f.Accept(85.0));
  TEST_ASSERT_TRUE(f.Accept(85.0));
}

void test_other_values_always_pass() {
  PowerOnFilter f;
  TEST_ASSERT_TRUE(f.Accept(84.9375));
  TEST_ASSERT_TRUE(f.Accept(-10.0));
  TEST_ASSERT_TRUE(f.Accept(120.0));
}

void test_alarm_disabled_by_default() {
  HighAlarm a;
  TEST_ASSERT_FALSE(a.enabled());
  TEST_ASSERT_FALSE(a.Update(200));
}

void test_alarm_trips_at_threshold_and_clears_below_hysteresis() {
  HighAlarm a(90, 3);
  TEST_ASSERT_FALSE(a.Update(89.9));
  TEST_ASSERT_TRUE(a.Update(90.0));
  TEST_ASSERT_TRUE(a.Update(88.0));   // inside hysteresis band
  TEST_ASSERT_TRUE(a.Update(87.0));   // exactly threshold - hysteresis: still on
  TEST_ASSERT_FALSE(a.Update(86.9));
}

void test_alarm_clear_and_reconfigure() {
  HighAlarm a(50, 2);
  TEST_ASSERT_TRUE(a.Update(60));
  a.Clear();
  TEST_ASSERT_FALSE(a.active());
  TEST_ASSERT_TRUE(a.Update(60));
  a.Configure(0, 2);  // disable
  TEST_ASSERT_FALSE(a.active());
  TEST_ASSERT_FALSE(a.Update(60));
  a.Configure(55, -1);  // negative hysteresis treated as zero
  TEST_ASSERT_TRUE(a.Update(55));
  TEST_ASSERT_FALSE(a.Update(54.99));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_kelvin_conversion);
  RUN_TEST(test_power_on_value_dropped_as_first_reading);
  RUN_TEST(test_power_on_value_dropped_as_jump);
  RUN_TEST(test_real_85_reached_gradually_passes);
  RUN_TEST(test_other_values_always_pass);
  RUN_TEST(test_alarm_disabled_by_default);
  RUN_TEST(test_alarm_trips_at_threshold_and_clears_below_hysteresis);
  RUN_TEST(test_alarm_clear_and_reconfigure);
  return UNITY_END();
}
