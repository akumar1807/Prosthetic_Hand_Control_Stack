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

// --- Timing and Filter Parameters ---
unsigned long lastUpdate = 0; 
const unsigned long updateInterval = 20; // 20 ms -> 50 Hz update rate (smoother speed estimation)

const float COUNTS_PER_REV = 4096.0f;
const float TWO_PI_F       = 6.2831853f;
const float LPF_ALPHA      = 0.25f;      // Filter weight (0.05 = heavily filtered, 1.0 = raw)

// --- Tracking State Variables ---
uint16_t lastRawPos = 0;
float filteredRpm   = 0.0f;
float filteredRadS  = 0.0f;

// Forward declaration
void runDashboard();

void setup() { 
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("\n--- ESP32 Control Dashboard Initializing ---"); 

  // 1. Configure and Activate Driver Enable Pins 
  pinMode(IBT2_REN, OUTPUT); 
  pinMode(IBT2_LEN, OUTPUT); 
  digitalWrite(IBT2_REN, HIGH); // Enable Right side of H-Bridge 
  digitalWrite(IBT2_LEN, HIGH); // Enable Left side of H-Bridge 
  Serial.println("[OK] IBT_2 Enable pins set HIGH."); 

  // 2. Initialize I2C Bus 
  Wire.begin(I2C_SDA, I2C_SCL); 
  Wire.setClock(400000); // 400 kHz Fast I2C mode

  // 3. Initialize AS5600 Encoder 
  as5600.begin(); 
  if (as5600.isConnected()) { 
    Serial.println("[OK] AS5600 Encoder connected."); 
    lastRawPos = as5600.readAngle(); // Seed initial angle reading
  } else { 
    Serial.println("[ERROR] AS5600 Encoder not found! Check I2C wiring."); 
  } 

  // 4. Initialize INA226 Power Monitor 
  if (ina226.begin()) { 
    ina226.setMaxCurrentShunt(10.0, 0.1); // 0.1 Ohm for standard R100 shunt resistor 
    Serial.println("[OK] INA226 Current Monitor connected."); 
  } else { 
    Serial.println("[ERROR] INA226 not found! Check I2C address and wiring."); 
  } 

  // 5. Configure ESP32 PWM Control Channels for IBT_2 
  ledcAttachChannel(IBT2_RPWM, PWM_FREQ, PWM_RES, CH_FORWARD); 
  ledcAttachChannel(IBT2_LPWM, PWM_FREQ, PWM_RES, CH_REVERSE); 

  Serial.println("System Ready. Driving motor forward...\n"); 
  
  // Set motor to drive forward at ~47% duty cycle
  ledcWriteChannel(CH_REVERSE, 0); 
  ledcWriteChannel(CH_FORWARD, 120); 

  lastUpdate = millis();
} 

void loop() { 
  // Continuously execute the sensor sampling and telemetry update
  runDashboard(); 
} 

void runDashboard() { 
  unsigned long currentTime = millis();
  unsigned long elapsedMs = currentTime - lastUpdate;

  if (elapsedMs >= updateInterval) { 
    float dt = (float)elapsedMs / 1000.0f; // Convert elapsed time to seconds
    lastUpdate = currentTime; 

    // 1. Read Current Raw Position (0 to 4095)
    uint16_t currentRawPos = as5600.readAngle(); 

    // 2. Compute Angular Delta with Wrap-around Correction
    int32_t deltaCounts = (int32_t)currentRawPos - (int32_t)lastRawPos;
    lastRawPos = currentRawPos; 

    // Handle 0 <-> 4095 transition (assuming < 180 deg rotation per dt)
    if (deltaCounts > 2048) {
      deltaCounts -= 4096;
    } else if (deltaCounts < -2048) {
      deltaCounts += 4096;
    }

    // 3. Compute Instantaneous Raw Speed
    float revsPerSec  = ((float)deltaCounts / COUNTS_PER_REV) / dt;
    float rawRpm      = revsPerSec * 60.0f;
    float rawRadS     = revsPerSec * TWO_PI_F;

    // 4. Low-Pass Filter (Exponential Moving Average)
    filteredRpm  = (LPF_ALPHA * rawRpm)  + ((1.0f - LPF_ALPHA) * filteredRpm);
    filteredRadS = (LPF_ALPHA * rawRadS) + ((1.0f - LPF_ALPHA) * filteredRadS);

    // 5. Read Power Specs from INA226 
    float voltage      = ina226.getBusVoltage(); 
    float current_mA   = ina226.getCurrent_mA(); 
    float current_amps = current_mA / 1000.0f; 

    // 6. Formatted Telemetry Output
    float current_deg = (currentRawPos * 360.0f) / 4095.0f;

    Serial.print("POS: "); 
    Serial.print(current_deg, 1); 
    Serial.print("° \t| SPEED: "); 
    Serial.print(filteredRpm, 1); 
    Serial.print(" RPM (");
    Serial.print(filteredRadS, 2);
    Serial.print(" rad/s) \t| VOLT: "); 
    Serial.print(voltage, 2); 
    Serial.print("V \t| CURR: "); 
    Serial.print(current_amps, 3); 
    Serial.println(" A"); 
  } 
}