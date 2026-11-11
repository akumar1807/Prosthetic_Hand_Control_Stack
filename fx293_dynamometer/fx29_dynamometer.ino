#include <Arduino.h>

// =======================================================
// 1. HARDWARE DEFINITIONS & RESISTOR DIVIDER RATIO
// =======================================================
// Recommended input-only ADC1 pin (e.g., GPIO 34, 35, or 36/VP)
const int LOADCELL_ADC_PIN = 34;

// Resistor divider values: R1 = 12k, R2 = 12k + 12k = 24k
const float R1 = 12000.0;
const float R2 = 24000.0;
const float DIVIDER_SCALE = R2 / (R1 + R2); // 24 / 36 = 0.6667

// =======================================================
// 2. SENSOR SPECIFICATIONS (FX293X)
// =======================================================
// Full-scale capacity from your sensor's part code (e.g. 50, 100, 200, etc.)
// Check the suffix on your sensor: e.g. 0025-L (25 lbf), 0050-L (50 lbf), 0100-L (100 lbf)
const float RATED_CAPACITY_LBF = 25.0; // Change to your rated capacity in lbf

// FX293X output parameters at 5V supply:
// 0% load = 0.5V (500 mV), 100% load = 4.5V (4500 mV) -> Span = 4.0V (4000 mV)
const float NOMINAL_SPAN_MV = 4000.0; 

// Calibrated zero baseline (in mV at the sensor before the divider)
float zeroOffset_sensor_mV = 500.0; 

// =======================================================
// 3. HELPER FUNCTIONS
// =======================================================

// Reads and converts ADC mV back to the actual FX29 output voltage
float readSensorOutputMilliVolts() {
  const int NUM_SAMPLES = 64;
  double sum_pin_mV = 0.0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum_pin_mV += (double)analogReadMilliVolts(LOADCELL_ADC_PIN);
    delayMicroseconds(250);
  }

  float avgPin_mV = (float)(sum_pin_mV / (double)NUM_SAMPLES);
  // Restore true voltage output from FX29 before divider
  return avgPin_mV / DIVIDER_SCALE;
}

// Tare function to zero the dynamometer at startup or on demand
void tareDynamometer() {
  Serial.println("\nTaring Load Cell... Keep sensor unloaded.");
  delay(200);

  double sum_mV = 0.0;
  for (int i = 0; i < 20; i++) {
    sum_mV += (double)readSensorOutputMilliVolts();
    delay(20);
  }

  zeroOffset_sensor_mV = (float)(sum_mV / 20.0);
  Serial.printf("Tare complete! Baseline Sensor Output: %.2f mV (Nominal: 500 mV)\n\n", zeroOffset_sensor_mV);
}

// =======================================================
// 4. SETUP & MAIN LOOP
// =======================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  ESP32 FX293X Analog Dynamometer / Load Cell");
  Serial.println("==========================================");

  // Configure ADC for 12-bit (0-4095) with 11dB attenuation (up to ~3.1V input)
  analogReadResolution(12);
  analogSetPinAttenuation(LOADCELL_ADC_PIN, ADC_11db);

  // Auto-zero baseline
  tareDynamometer();

  Serial.println("Send 't' via Serial to re-tare anytime.");
  Serial.println("Sensor_mV\tForce (lbf)\tForce (N)\tWeight (kg)");
  Serial.println("---------------------------------------------------------");
}

void loop() {
  // Check for serial user command to re-tare
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 't' || cmd == 'T') {
      tareDynamometer();
    }
  }

  // 1. Read current voltage produced by the FX29
  float current_sensor_mV = readSensorOutputMilliVolts();

  // 2. Compute force based on span: Force = ((V_out - V_zero) / 4000 mV) * Rated_Capacity
  float delta_mV = current_sensor_mV - zeroOffset_sensor_mV;
  if (delta_mV < 0.0f) delta_mV = 0.0f; // Clip small noise drift below zero

  float force_lbf = (delta_mV / NOMINAL_SPAN_MV) * RATED_CAPACITY_LBF;
  float force_N   = force_lbf * 4.44822f;          // 1 lbf = 4.44822 N
  float mass_kg   = force_N / 9.80665f;            // F = m * g -> m = F / 9.80665

  // Print results
  Serial.printf("%.1f mV\t\t%.3f lbf\t%.3f N\t%.3f kg\n",
                current_sensor_mV, force_lbf, force_N, mass_kg);

  delay(100); // 10 Hz telemetry loop
}

// NOTE!!!! UNCOMMENT ONCE CALIBRATION IS COMPLETE
// // --- Paste your calibrated slope here ---
// const float CALIBRATED_SLOPE_MV_PER_G = 0.1542; // Example value: S from Step 4

// float zeroBaseline_mV = 500.0; // Overwritten by tareDynamometer() at startup

// void tareDynamometer() {
//   double sum = 0;
//   for (int i = 0; i < 50; i++) {
//     sum += readSensorVoltage_mV();
//     delay(10);
//   }
//   zeroBaseline_mV = (float)(sum / 50.0);
// }

// void loop() {
//   float current_mV = readSensorVoltage_mV();
//   float delta_mV = current_mV - zeroBaseline_mV;
//   if (delta_mV < 0.0f) delta_mV = 0.0f;

//   // Mass in grams
//   float mass_grams = delta_mV / CALIBRATED_SLOPE_MV_PER_G;

//   // Force in Newtons: F = m * g (m in kg)
//   float force_N = (mass_grams / 1000.0f) * 9.80665f;

//   Serial.printf("%.1f mV | Mass: %.1f g | Force: %.3f N\n", current_mV, mass_grams, force_N);
//   delay(100);
// }