#include <Wire.h> 
#include <AS5600.h> 
#include <INA226.h> 
 
// --- Pin Definitions --- 
#define I2C_SDA     21 
#define I2C_SCL     22 
#define IBT2_RPWM   25 
#define IBT2_LPWM   26 
 
// --- New Enable Pin Definitions (Option 2) --- 
#define IBT2_REN    18  // Connected to R_EN on IBT_2 
#define IBT2_LEN    19  // Connected to L_EN on IBT_2 
 
// --- PWM Hardware Settings for ESP32 --- 
#define PWM_FREQ    5000 
#define PWM_RES     8  // 8-bit resolution (values from 0 to 255) 
#define CH_FORWARD  0 
#define CH_REVERSE  1 
 
// --- Sensor Objects --- 
AS5600 as5600; 
INA226 ina226(0x40); // 0x40 is the standard default I2C address for INA226 
 
unsigned long lastUpdate = 0; 
const int updateInterval = 250; // Read sensors every 250ms 
 
void setup() { 
  Serial.begin(115200); 
  delay(1000); 
  Serial.println("\n--- ESP32 Control Dashboard Initializing ---"); 
 
  // 1. Configure and Activating Driver Enable Pins 
  pinMode(IBT2_REN, OUTPUT); 
  pinMode(IBT2_LEN, OUTPUT); 
  digitalWrite(IBT2_REN, HIGH); // Turn on Right side of H-Bridge 
  digitalWrite(IBT2_LEN, HIGH); // Turn on Left side of H-Bridge 
  Serial.println("[OK] IBT_2 Enable pins set HIGH."); 
 
  // 2. Initialize I2C Bus (This configures the pins for BOTH sensors) 
  Wire.begin(I2C_SDA, I2C_SCL); 
 
  // 3. Initialize AS5600 Encoder 
  as5600.begin(); 
  if (as5600.isConnected()) { 
    Serial.println("[OK] AS5600 Encoder connected."); 
  } else { 
    Serial.println("[ERROR] AS5600 Encoder not found! Check I2C wiring."); 
  } 
 
  // 4. Initialize INA226 Power Monitor (Corrected Line) 
  if (ina226.begin()) { 
    ina226.setMaxCurrentShunt(10.0, 0.1); // 0.1 Ohm for R100 shunt resistor 
    Serial.println("[OK] INA226 Current Monitor connected."); 
  } else { 
    Serial.println("[ERROR] INA226 not found! Check I2C address and wiring."); 
  } 
 
  // 5. Configure ESP32 PWM Control Channels for IBT_2 
  ledcAttachChannel(IBT2_RPWM, PWM_FREQ, PWM_RES, CH_FORWARD); 
  ledcAttachChannel(IBT2_LPWM, PWM_FREQ, PWM_RES, CH_REVERSE); 
 
  Serial.println("System Ready. Beginning Motor Sweep Loop...\n"); 
  delay(2000); 
} 
 
 
void loop() { 
  // --- STAGE 1: Move Motor Forward --- 
  Serial.println(">>> Action: Driving Forward..."); 
  ledcWriteChannel(CH_REVERSE, 0);       // Safety: Turn off opposite direction first 
  ledcWriteChannel(CH_FORWARD, 120);     // Set speed (~47% duty cycle) 
  
  // Run for 3 seconds while monitoring status 
  unsigned long stageStart = millis(); 
  while (millis() - stageStart < 3000) { 
    runDashboard(); 
  } 
 
  // --- STAGE 2: Stop and Rest --- 
  Serial.println(">>> Action: Braking / Coasting..."); 
  ledcWriteChannel(CH_FORWARD, 0); 
  ledcWriteChannel(CH_REVERSE, 0); 
  
  stageStart = millis(); 
  while (millis() - stageStart < 1500) { 
    runDashboard(); 
  } 
 
  // --- STAGE 3: Move Motor Backward --- 
  Serial.println(">>> Action: Driving Backward..."); 
  ledcWriteChannel(CH_FORWARD, 0);       // Safety: Turn off opposite direction first 
  ledcWriteChannel(CH_REVERSE, 120);     // Set speed (~47% duty cycle) 
  
  stageStart = millis(); 
  while (millis() - stageStart < 3000) { 
    runDashboard(); 
  } 
 
  // --- STAGE 4: Stop and Rest --- 
  Serial.println(">>> Action: Braking / Coasting..."); 
  ledcWriteChannel(CH_FORWARD, 0); 
  ledcWriteChannel(CH_REVERSE, 0); 
  
  stageStart = millis(); 
  while (millis() - stageStart < 1500) { 
    runDashboard(); 
  } 
} 
 
void runDashboard() { 
  if (millis() - lastUpdate >= updateInterval) { 
    lastUpdate = millis(); 
 
    // Read Encoder Position 
    uint16_t raw_pos = as5600.readAngle(); 
    float angle_deg = raw_pos * 360.0 / 4095.0; 
 
    // Read Power Specs from INA226 
    float voltage = ina226.getBusVoltage(); 
    float current_mA = ina226.getCurrent_mA(); 
    float current_amps = current_mA / 1000.0; 
    // Print to Serial Monitor in a clean telemetry layout 
    Serial.print("POS: "); 
    Serial.print(angle_deg, 1); 
    Serial.print("° \t| VOLT: "); 
    Serial.print(voltage, 2); 
    Serial.print("V \t| CURR: "); 
    Serial.print(current_amps, 3); 
    Serial.println(" A"); 
  } 
}