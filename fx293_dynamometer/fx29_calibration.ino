#include <Arduino.h>

const int LOADCELL_ADC_PIN = 34; // GPIO 34 (ADC1)

// Your resistor divider: R1 = 12k, R2 = 12k + 12k = 24k
const float R1 = 12000.0;
const float R2 = 24000.0;
const float DIVIDER_SCALE = R2 / (R1 + R2); // 24k / 36k = 0.6667

// Read averaged sensor voltage (in mV) restored before the voltage divider
float readSensorVoltage_mV() {
  const int SAMPLES = 100;
  double sum_pin_mV = 0.0;

  for (int i = 0; i < SAMPLES; i++) {
    sum_pin_mV += (double)analogReadMilliVolts(LOADCELL_ADC_PIN);
    delay(2);
  }

  float avgPin_mV = (float)(sum_pin_mV / (double)SAMPLES);
  return avgPin_mV / DIVIDER_SCALE; // Reconstructs actual FX29 output voltage
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  analogReadResolution(12);
  analogSetPinAttenuation(LOADCELL_ADC_PIN, ADC_11db);

  Serial.println("\n=============================================");
  Serial.println("  FX293X Calibration Utility (50g Increments)");
  Serial.println("=============================================");
  Serial.println("Commands:");
  Serial.println("  'r' -> Capture 100-sample averaged voltage");
  Serial.println("---------------------------------------------");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'r' || cmd == 'R') {
      Serial.print("Sampling... ");
      float v_mV = readSensorVoltage_mV();
      Serial.printf("Measured Sensor Output: %.2f mV\n", v_mV);
    }
  }
}