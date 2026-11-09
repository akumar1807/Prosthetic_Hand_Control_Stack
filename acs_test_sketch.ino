#include <Arduino.h>

// ==========================================
// 1. PIN DEFINITIONS & DIRECTION FLAGS
// ==========================================
const int RPWM_PIN = 25; // Right PWM (Forward speed signal)
const int LPWM_PIN = 26; // Left PWM (Reverse speed signal)
const int R_EN_PIN = 18; // Right Enable on GPIO 18
const int L_EN_PIN = 19; // Left Enable on GPIO 19

const int ENCODER_A_PIN = 32; // Encoder Channel A
const int ENCODER_B_PIN = 33; // Encoder Channel B

const int CURRENT_SENSE_PIN = 34; // ACS712 Analog Output (ADC1_CH6)

const bool INVERT_MOTOR_DIRECTION = true;
// 2. Set INVERT_ENCODER_DIR to 'true' to flip negative velocity readings to positive
const bool INVERT_ENCODER_DIR = true;

// ==========================================
// 2. MOTOR & ENCODER PARAMETERS
// ==========================================
// Adjust COUNTS_PER_REV to match your specific DCGM-3865 gearbox & PPR
// Common 240 RPM 3865 planetary/spur motors: 11 PPR * ~25:1 gear ratio * 2 (Rising/Falling on Ch A) ≈ 550
const float COUNTS_PER_REV = 546.0; 

volatile long encoderCounts = 0;
long prevEncoderCounts = 0;

// ==========================================
// 3. ACS712 & ADC PARAMETERS
// ==========================================
// Select your ACS712 module sensitivity:
// - 5A Module:  0.185 V/A
// - 20A Module: 0.100 V/A
// - 30A Module: 0.066 V/A
const float ACS712_SENSITIVITY = 0.185; 

// Voltage divider ratio: R1 = 10k, R2 = 20k -> Scale = 20 / (10 + 20) = 0.6667
// Set to 1.0 ONLY if you are NOT using a voltage divider (not recommended for ESP32)
// const float DIVIDER_SCALE = 20.0 / (10.0 + 20.0);
// const float DIVIDER_SCALE = 1.0; // 1 because voltage divider is not being used.

// float zeroCurrentVoltage_mV = 1667.0; // Auto-calibrated in setup()

// Calibrated sensitivity (accounts for USB <5V rail and ESP32 ADC roll-off at 2.5V)
// const float ACS712_SENSITIVITY = 0.156; 
const float DIVIDER_SCALE = 1.0;

float zeroCurrentVoltage_mV = 2500.0;
float filteredCurrentAmps = 0.0;

// ==========================================
// 4. TIMING & TEST SEQUENCE VARIABLES
// ==========================================
unsigned long prevTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 100; // 10 Hz update rate

// ==========================================
// 5. INTERRUPT SERVICE ROUTINE (ISR)
// ==========================================
void IRAM_ATTR readEncoderISR() {
  int stateA = digitalRead(ENCODER_A_PIN);
  int stateB = digitalRead(ENCODER_B_PIN);

  // Determine direction using quadrature phase difference on Both Edges of Channel A
  int step = (stateA == stateB) ? -1 : 1;
  
  if (INVERT_ENCODER_DIR) {
    step = -step;
  }
  
  encoderCounts += step;
}

// ==========================================
// 6. HELPER FUNCTIONS
// ==========================================

// Set motor speed from -255 (Full Reverse) to +255 (Full Forward)
void setMotorPWM(int pwmValue) {
  pwmValue = constrain(pwmValue, -255, 255);

  if (INVERT_MOTOR_DIRECTION) {
    pwmValue = -pwmValue;
  }

  // Pure digitalWrite test (100% full speed only, no variable speed control):
  if (pwmValue > 0) {
    digitalWrite(RPWM_PIN, HIGH);
    digitalWrite(LPWM_PIN, LOW);
  } else if (pwmValue < 0) {
    digitalWrite(RPWM_PIN, LOW);
    digitalWrite(LPWM_PIN, HIGH);
  } else {
    digitalWrite(RPWM_PIN, LOW);
    digitalWrite(LPWM_PIN, LOW);
  }
}

void calibrateCurrentSensor() {
  Serial.println("Calibrating ACS712 Zero-Current Offset (Motor OFF)...");
  setMotorPWM(0);
  delay(500);

  double sum_mV = 0.0;
  const int numSamples = 2000; // Higher sample count for sub-millivolt zero precision
  for (int i = 0; i < numSamples; i++) {
    sum_mV += (double)analogReadMilliVolts(CURRENT_SENSE_PIN);
    delayMicroseconds(500);
  }
  zeroCurrentVoltage_mV = (float)(sum_mV / (double)numSamples);
  Serial.printf("Calibration Complete! Zero-Point = %.3f mV\n\n", zeroCurrentVoltage_mV);
}

void readCurrentSensor(int &rawADCOut, float &currentAmpsOut) {
  const int samples = 50; 
  long rawSum = 0;
  long mVSum = 0;

  // Spread 50 samples across 10ms so brush ripple cancels out
  for (int i = 0; i < samples; i++) {
    rawSum += analogRead(CURRENT_SENSE_PIN);
    mVSum += analogReadMilliVolts(CURRENT_SENSE_PIN);
    delayMicroseconds(200);
  }

  rawADCOut = rawSum / samples;
  float avgPin_mV = (float)mVSum / samples;

  float deltaPin_V = (avgPin_mV - zeroCurrentVoltage_mV) / 1000.0;
  float deltaSensor_V = deltaPin_V / DIVIDER_SCALE;
  float instantAmps = deltaSensor_V / ACS712_SENSITIVITY;

  // Exponential Moving Average (EMA) low-pass filter
  if (filteredCurrentAmps == 0.0) {
    filteredCurrentAmps = instantAmps;
  } else {
    filteredCurrentAmps = (0.15 * instantAmps) + (0.85 * filteredCurrentAmps);
  }

  currentAmpsOut = filteredCurrentAmps;
}

// ==========================================
// 7. SETUP & MAIN LOOP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configure BTS7960N pins
  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);

  // Enable both half-bridges on the BTS7960N
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);
  setMotorPWM(0);

  // Configure Encoder pins with internal pull-ups
  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), readEncoderISR, CHANGE);

  // Configure ESP32 12-bit ADC with 11dB attenuation (0V to ~3.1V measurable range)
  analogReadResolution(12);
  analogSetPinAttenuation(CURRENT_SENSE_PIN, ADC_11db);

  // Calibrate current sensor baseline
  calibrateCurrentSensor();

  Serial.println("PWM\tCounts\tVel (rad/s)\tCurrent (A)");
  Serial.println("------------------------------------------------------------");
  prevTelemetryTime = millis();
}

void loop() {
  unsigned long now = millis();

  // 1. Continuous Rotation at a Fixed PWM Value (-255 to +255)
  const int targetPWM = 100; // ~39% duty cycle in the forward direction
  setMotorPWM(targetPWM);

  // 2. Sample and Print Telemetry at 10 Hz (every 100 ms)
  if (now - prevTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    float dt = (now - prevTelemetryTime) / 1000.0; // Elapsed time in seconds
    prevTelemetryTime = now;

    // Safely copy volatile encoder count
    noInterrupts();
    long currentCounts = encoderCounts;
    interrupts();

    // Calculate angular velocity in rad/s
    long deltaCounts = currentCounts - prevEncoderCounts;
    prevEncoderCounts = currentCounts;
    float velocityRadPerSec = ((float)deltaCounts / COUNTS_PER_REV) * TWO_PI / dt;

    // Read current sensor (Raw 0-4095 and calibrated Amperes)
    int rawADC = 0;
    float currentAmps = 0.0;
    readCurrentSensor(rawADC, currentAmps);

    // Output formatted for Serial Monitor or Serial Plotter
    Serial.printf("%d\t%ld\t%.3f\t%.3f\n", 
                  targetPWM, currentCounts, velocityRadPerSec, currentAmps);
  }
}