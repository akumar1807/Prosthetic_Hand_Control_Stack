#include <Wire.h> 
#include <AS5600.h>
#include <INA226.h> 

// --- Pin Definitions --- 
#define I2C_SDA     21 
#define I2C_SCL     22 
#define IBT2_RPWM   25 
#define IBT2_LPWM   26 

// --- Enable Pin Definitions --- 
#define IBT2_REN    18  // Connected to R_EN on IBT_2 
#define IBT2_LEN    19  // Connected to L_EN on IBT_2 

// --- PWM Hardware Settings for ESP32 --- 
#define PWM_FREQ    5000 
#define PWM_RES     8   // 8-bit resolution (0 to 255) 
#define CH_FORWARD  0 
#define CH_REVERSE  1 

// --- Sensor Objects --- 
AS5600 as5600; 
INA226 ina226(0x40); 

// --- Control Loop Timing ---
unsigned long lastUpdate = 0; 
const unsigned long updateInterval = 20; // 20 ms -> 50 Hz control loop

// --- PID Controller Parameters ---
float targetAngle = 90.0; // Desired resting position in degrees

// Arbitrary initial gains - these will need tuning for your specific motor dynamics
float Kp = 5.0; 
float Ki = 1.0; 
float Kd = 0.2; 

// PID State Variables
float errorIntegral = 0.0;
float lastError = 0.0;
const float MAX_INTEGRAL = 500.0; // Anti-windup threshold

void setup() { 
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("\n--- ESP32 Closed-Loop PID Controller Initializing ---"); 

  // 1. Configure and Activate Driver Enable Pins 
  pinMode(IBT2_REN, OUTPUT); 
  pinMode(IBT2_LEN, OUTPUT); 
  digitalWrite(IBT2_REN, HIGH); 
  digitalWrite(IBT2_LEN, HIGH); 

  // 2. Initialize I2C Bus 
  Wire.begin(I2C_SDA, I2C_SCL); 
  Wire.setClock(400000); 

  // 3. Initialize AS5600 Encoder 
  as5600.begin(); 
  if (as5600.isConnected()) { 
    Serial.println("[OK] AS5600 Encoder connected."); 
  } else { 
    Serial.println("[ERROR] AS5600 Encoder not found! Check I2C wiring."); 
  } 

  // 4. Initialize INA226 Power Monitor 
  if (ina226.begin()) { 
    ina226.setMaxCurrentShunt(10.0, 0.1); 
    Serial.println("[OK] INA226 Current Monitor connected."); 
  } else { 
    Serial.println("[ERROR] INA226 not found!"); 
  } 

  // 5. Configure ESP32 PWM Control Channels 
  ledcAttachChannel(IBT2_RPWM, PWM_FREQ, PWM_RES, CH_FORWARD); 
  ledcAttachChannel(IBT2_LPWM, PWM_FREQ, PWM_RES, CH_REVERSE); 
  
  // Ensure motor is stopped at startup
  ledcWriteChannel(CH_FORWARD, 0);
  ledcWriteChannel(CH_REVERSE, 0);

  Serial.println("System Ready. Beginning PID Control Loop...\n"); 
  lastUpdate = millis();
} 

void loop() { 
  runPIDControl(); 
} 

void runPIDControl() { 
  unsigned long currentTime = millis();
  unsigned long elapsedMs = currentTime - lastUpdate;

  // Execute control math only on the strict 20ms interval
  if (elapsedMs >= updateInterval) { 
    float dt = (float)elapsedMs / 1000.0; // Convert to seconds
    lastUpdate = currentTime; 

    // 1. Read Current Position 
    uint16_t raw_pos = as5600.readAngle(); 
    float currentAngle = raw_pos * (360.0 / 4095.0); 

    // 2. Shortest Path Error Calculation
    float error = targetAngle - currentAngle;
    
    // Wrap error bounds to strictly [-180, 180] degrees
    if (error > 180.0) {
      error -= 360.0;
    } else if (error < -180.0) {
      error += 360.0;
    }

    // 3. Calculate PID Terms
    // Proportional
    float pTerm = Kp * error;

    // Integral with anti-windup clamping
    errorIntegral += (error * dt);
    if (errorIntegral > MAX_INTEGRAL) errorIntegral = MAX_INTEGRAL;
    if (errorIntegral < -MAX_INTEGRAL) errorIntegral = -MAX_INTEGRAL;
    float iTerm = Ki * errorIntegral;

    // Derivative (rate of change of error)
    float dTerm = Kd * ((error - lastError) / dt);
    lastError = error;

    // Sum control effort
    float controlOutput = pTerm + iTerm + dTerm;

    // 4. Actuation Mapping (Map output to 8-bit PWM: 0-255)
    int pwmValue = abs((int)controlOutput);
    if (pwmValue > 255) pwmValue = 255; // Clamp hardware limits
    
    // Create a small deadband to prevent motor buzzing at the exact target
    if (abs(error) < 1.5) {
      pwmValue = 0; 
      errorIntegral = 0; // Clear integral memory at target to prevent drift
    }

    // Command the H-Bridge
    if (controlOutput > 0) {
      ledcWriteChannel(CH_REVERSE, 0); // Safety block
      ledcWriteChannel(CH_FORWARD, pwmValue);
    } else if (controlOutput < 0) {
      ledcWriteChannel(CH_FORWARD, 0);
      ledcWriteChannel(CH_REVERSE, pwmValue);
    } else {
      ledcWriteChannel(CH_FORWARD, 0);
      ledcWriteChannel(CH_REVERSE, 0);
    }

    // 5. Telemetry output for Arduino Serial Plotter
    Serial.print("Target:"); 
    Serial.print(targetAngle); 
    Serial.print(",Current:"); 
    Serial.print(currentAngle); 
    Serial.print(",Error:"); 
    Serial.print(error); 
    Serial.print(",PWM:"); 
    Serial.println(controlOutput > 0 ? pwmValue : -pwmValue); // Show direction as sign
  } 
}