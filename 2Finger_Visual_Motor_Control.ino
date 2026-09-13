#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <INA226.h>

// =====================================================
// WIFI & UDP SETTINGS
// =====================================================
const char* ssid = "WIRED_Lab";
const char* password = "Wiredlab123654";
WiFiUDP udp;
const int udpPort = 4210;
char packetBuffer[255]; 

// =====================================================
// BTS7960 PIN MAP (ESP32)
// =====================================================
const uint8_t LPWM = 26;   // BTS7960 LPWM -> OPEN direction
const uint8_t RPWM = 27;   // BTS7960 RPWM -> CLOSE direction
const uint8_t R_EN = 33;   // BTS7960 R_EN
const uint8_t L_EN = 32;   // BTS7960 L_EN

// =====================================================
// MOTOR & SAFETY SETTINGS
// =====================================================
const unsigned long OPEN_TIME_MS  = 90;  // Open pulse duration
const unsigned long CLOSE_TIME_MS = 600;  // Close pulse duration
const uint8_t TARGET_SPEED        = 200;  // Peak PWM duty (0-255)
const uint8_t MIN_SPEED           = 40;   // Minimum start/end PWM duty

// Ramp configuration: percentage of total run duration dedicated to ramping
const float RAMP_FRACTION = 0.25f; 

// Current limit configuration
const float CURRENT_LIMIT_A          = 0.8; // Cut-off threshold in Amps
const unsigned long INRUSH_IGNORE_MS = 30;  // Brief blanking window for motor commutation

// =====================================================
// MOTOR STATE
// =====================================================
bool motorRunning = false;
unsigned long motorStartTime = 0;
unsigned long motorRunTime   = 0;

enum MotorDirection {
  MOTOR_STOP,
  MOTOR_OPEN,
  MOTOR_CLOSE
};
MotorDirection motorDirection = MOTOR_STOP;

// =====================================================
// INA226 CURRENT SENSOR CONFIG
// =====================================================
const uint8_t INA_ADDRESS      = 0x40;
const float   SHUNT_RESISTANCE = 0.1;  // 0.1 Ohm shunt (R100)
const float   MAX_EXPECTED_A   = 1.5;  // Headroom for calibration

INA226 ina(INA_ADDRESS);

// =====================================================
// MOTOR HARDWARE DRIVERS
// =====================================================
void setBridgeDuty(MotorDirection dir, uint8_t duty) {
  if (dir == MOTOR_OPEN) {
    analogWrite(RPWM, 0);
    analogWrite(LPWM, duty);
  } else if (dir == MOTOR_CLOSE) {
    analogWrite(LPWM, 0);
    analogWrite(RPWM, duty);
  } else {
    analogWrite(LPWM, 0);
    analogWrite(RPWM, 0);
  }
}

void stopMotor() {
  setBridgeDuty(MOTOR_STOP, 0);
  motorRunning = false;
  motorDirection = MOTOR_STOP;
  Serial.println("Motor STOP");
}

void openMotor() {
  motorDirection = MOTOR_OPEN;
  motorStartTime = millis();
  motorRunTime   = OPEN_TIME_MS;
  motorRunning   = true;
  setBridgeDuty(MOTOR_OPEN, MIN_SPEED);
  Serial.println("Action: OPENING (Ramped)");
}

void closeMotor() {
  motorDirection = MOTOR_CLOSE;
  motorStartTime = millis();
  motorRunTime   = CLOSE_TIME_MS;
  motorRunning   = true;
  setBridgeDuty(MOTOR_CLOSE, MIN_SPEED);
  Serial.println("Action: CLOSING (Ramped)");
}

// =====================================================
// PROFILE UPDATE ENGINE
// =====================================================
void updateMotorProfile() {
  if (!motorRunning) return;

  unsigned long elapsed = millis() - motorStartTime;

  // Pulse completed
  if (elapsed >= motorRunTime) {
    stopMotor();
    return;
  }

  unsigned long rampUpDuration   = (unsigned long)(motorRunTime * RAMP_FRACTION);
  unsigned long rampDownDuration = (unsigned long)(motorRunTime * RAMP_FRACTION);
  unsigned long rampDownStart    = motorRunTime - rampDownDuration;

  uint8_t currentDuty = TARGET_SPEED;

  if (elapsed < rampUpDuration) {
    // 1. RAMP UP PHASE
    currentDuty = map(elapsed, 0, rampUpDuration, MIN_SPEED, TARGET_SPEED);
  } else if (elapsed >= rampDownStart) {
    // 2. RAMP DOWN PHASE
    currentDuty = map(elapsed, rampDownStart, motorRunTime, TARGET_SPEED, MIN_SPEED);
  } else {
    // 3. CRUISE PHASE
    currentDuty = TARGET_SPEED;
  }

  setBridgeDuty(motorDirection, currentDuty);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  // Initialize Motor Pins
  pinMode(LPWM, OUTPUT);
  pinMode(RPWM, OUTPUT);
  pinMode(R_EN, OUTPUT);
  pinMode(L_EN, OUTPUT);

  digitalWrite(R_EN, HIGH);
  digitalWrite(L_EN, HIGH);
  stopMotor();

  // Initialize INA226
  Wire.begin(21, 22);
  if (!ina.begin()) {
    Serial.println("ERROR: INA226 not detected. Check I2C wiring (SDA=21, SCL=22).");
    // Don't freeze completely, allow WiFi to connect for debugging
  } else {
    ina.setMaxCurrentShunt(MAX_EXPECTED_A, SHUNT_RESISTANCE);
    ina.setAverage(1); // 4-sample averaging
  }

  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP()); 
  
  // Begin listening on UDP
  udp.begin(udpPort);
  Serial.printf("Listening on UDP port %d\n", udpPort);
  
  Serial.println("==========================================");
  Serial.println("Motor Controller Ready (WiFi + Force Sensing)");
  Serial.println("==========================================");
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  // 1. Check for incoming WiFi (UDP) commands from MediaPipe
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = '\0';
    
    String command = String(packetBuffer);
    command.trim(); // Remove any stray whitespace
    
    // Ignore duplicate commands if motor is already executing that direction
    if (command == "CLOSE" && motorDirection != MOTOR_CLOSE) {
      closeMotor();
    } 
    else if (command == "OPEN" && motorDirection != MOTOR_OPEN) {
      openMotor();
    }
  }

  // 2. Calculate & apply real-time PWM duty curve
  updateMotorProfile();

  // 3. Read absolute current
  float current_A = fabs(ina.getCurrent());

  // 4. Overcurrent monitoring during active motion
  if (motorRunning) {
    if (millis() - motorStartTime > INRUSH_IGNORE_MS) {
      if (current_A >= CURRENT_LIMIT_A) {
        stopMotor();
        Serial.print(">> GRIP FORCE MET (Current: ");
        Serial.print(current_A, 3);
        Serial.println(" A). Motor shut down.");
      }
    }
  }

  // 5. Serial Telemetry (~5 Hz)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 200) {
    lastPrint = millis();
    Serial.print("I = ");
    Serial.print(current_A, 3);
    Serial.print(" A | State: ");
    Serial.println(motorRunning ? (motorDirection == MOTOR_CLOSE ? "CLOSING" : "OPENING") : "IDLE");
  }

  // 6. Serial Command Interface (Kept as fallback for manual keyboard control)
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command != '\n' && command != '\r') {
      switch (command) {
        case 'g':
        case 'G':
          // closeMotor();
          openMotor(); //CHANGE LATER, VICE VERSA
          break;
        case 'r':
        case 'R':
          // openMotor();
          closeMotor();
          break;
        case 's':
        case 'S':
          stopMotor();
          break;
      }
    }
  }

  delay(2);
}