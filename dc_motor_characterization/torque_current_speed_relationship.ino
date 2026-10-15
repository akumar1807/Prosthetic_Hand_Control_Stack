#include <Arduino.h>
#include "new_config.h" // Import all parameters from our separate config.h file

// ==========================================
// STATE VARIABLES
// ==========================================
volatile long encoderCount = 0;
long previousEncoderCount = 0;
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;

unsigned long testStartTime = 0;
unsigned long lastMeasureTime = 0;
unsigned long unwindStartTime = 0;

bool isTestRunning = false;
bool isUnwinding = false;

// ==========================================
// INTERRUPT SERVICE ROUTINE (ISR)
// ==========================================
void IRAM_ATTR readEncoderISR() {
  portENTER_CRITICAL_ISR(&synch);
  
  // Determine raw direction from Channel B
  bool stepForward = (digitalRead(ENCODER_B_PIN) == LOW);
  
  // Flip the counting direction in software if INVERT_ENCODER_DIR is true
  if (INVERT_ENCODER_DIR) {
    stepForward = !stepForward;
  }

  if (stepForward) {
    encoderCount++;
  } else {
    encoderCount--;
  }
  
  portEXIT_CRITICAL_ISR(&synch);
}

// ==========================================
// MOTOR CONTROL FUNCTION
// ==========================================
void setMotorPWM(int pwmValue) {
  pwmValue = constrain(pwmValue, -255, 255);
  
  // Flip physical motor spin direction if INVERT_MOTOR_DIRECTION is true
  if (INVERT_MOTOR_DIRECTION) {
    pwmValue = -pwmValue;
  }

  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  if (pwmValue > 0) {
    analogWrite(LPWM_PIN, 0);
    analogWrite(RPWM_PIN, pwmValue);
  } else if (pwmValue < 0) {
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, -pwmValue);
  } else {
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, 0);
  }
}

// ==========================================
// START A 5-SECOND WINDING & LOGGING RUN
// ==========================================
void startTestRun() {
  portENTER_CRITICAL(&synch);
  encoderCount = 0;
  portEXIT_CRITICAL(&synch);
  previousEncoderCount = 0;

  Serial.println("\n% --- START OF 5-SECOND RUN ---");
  Serial.println("% Time_s, PWM, Load_kg, Torque_Nm, Speed_RPM, Speed_RadPerSec");

  isTestRunning = true;
  isUnwinding = false;
  testStartTime = millis();
  lastMeasureTime = testStartTime;

  setMotorPWM(targetPWM);
}

// ==========================================
// START A 5-SECOND UNWIND RUN
// ==========================================
void startUnwindRun() {
  Serial.println("\n% --- START OF 5-SECOND UNWIND ---");
  isUnwinding = true;
  isTestRunning = false;
  unwindStartTime = millis();

  // Unwind at the exact same targetPWM magnitude, but in reverse (-targetPWM)
  setMotorPWM(-targetPWM);
}

// ==========================================
// SETUP FUNCTION
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(ENCODER_A_PIN, INPUT_PULLUP);
  pinMode(ENCODER_B_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_PIN), readEncoderISR, RISING);

  pinMode(R_EN_PIN, OUTPUT);
  pinMode(L_EN_PIN, OUTPUT);
  digitalWrite(R_EN_PIN, HIGH);
  digitalWrite(L_EN_PIN, HIGH);

  pinMode(RPWM_PIN, OUTPUT);
  pinMode(LPWM_PIN, OUTPUT);
  analogWriteFrequency(RPWM_PIN, PWM_FREQ);
  analogWriteFrequency(LPWM_PIN, PWM_FREQ);

  setMotorPWM(0);
  Serial.println("ESP32 Ready. Send mass (kg), 's' to wind (5s), 'r' to unwind (5s), or 'x' to stop.");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // --- PART A: Read Serial Commands ---
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); 

    if (input.equalsIgnoreCase("x")) {
      setMotorPWM(0);
      isTestRunning = false;
      isUnwinding = false;
      Serial.println("% --- MOTOR STOPPED ---");
    } 
    else if (!isTestRunning && !isUnwinding) {
      if (input.equalsIgnoreCase("s")) {
        startTestRun();
      } 
      else if (input.equalsIgnoreCase("r")) {
        startUnwindRun();
      } 
      else if (input.length() > 0) {
        float newMass = input.toFloat();
        if (newMass > 0.0 || input == "0" || input == "0.0") {
          loadMass_kg = newMass;
          Serial.print("% Updated Load Mass (kg): ");
          Serial.println(loadMass_kg, 3);
        }
      }
    }
  }

  // --- PART B: Execute 5-Second Winding & Data Logging ---
  if (isTestRunning) {
    unsigned long currentTime = millis();
    unsigned long elapsedTestTime = currentTime - testStartTime;
    unsigned long sampleElapsed = currentTime - lastMeasureTime;

    if (sampleElapsed >= MEASURE_INTERVAL) {
      long currentCount = 0;
      portENTER_CRITICAL(&synch);
      currentCount = encoderCount;
      portEXIT_CRITICAL(&synch);

      long deltaPulses = currentCount - previousEncoderCount;
      previousEncoderCount = currentCount;
      lastMeasureTime = currentTime;

      float timeSeconds = elapsedTestTime / 1000.0;
      float deltaTimeSeconds = sampleElapsed / 1000.0;
      float rps = (deltaPulses / PULSES_PER_REV) / deltaTimeSeconds;
      float rpm = rps * 60.0;
      float radPerSec = rps * 2.0 * PI;
      float torque_Nm = loadMass_kg * GRAVITY * pulleyRadius_m;

      Serial.print(timeSeconds, 2);   Serial.print(", ");
      Serial.print(targetPWM);        Serial.print(", ");
      Serial.print(loadMass_kg, 3);   Serial.print(", ");
      Serial.print(torque_Nm, 4);     Serial.print(", ");
      Serial.print(rpm, 2);           Serial.print(", ");
      Serial.println(radPerSec, 2);
    }

    if (elapsedTestTime >= TEST_DURATION_MS) {
      setMotorPWM(0);
      isTestRunning = false;
      Serial.println("% --- END OF 4-SECOND RUN ---");
    }
  }

  // --- PART C: Execute 5-Second Timed Unwinding ---
  if (isUnwinding) {
    unsigned long currentTime = millis();
    if (currentTime - unwindStartTime >= TEST_DURATION_MS) {
      setMotorPWM(0);
      isUnwinding = false;
      Serial.println("% --- END OF 4-SECOND UNWIND ---");
    }
  }
}