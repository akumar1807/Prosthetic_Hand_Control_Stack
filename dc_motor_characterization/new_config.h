#pragma once // Prevents the compiler from loading these variables multiple times

// ==========================================
// 1. PIN DEFINITIONS
// ==========================================
const int RPWM_PIN = 25; // Right PWM (Forward speed signal)
const int LPWM_PIN = 26; // Left PWM (Reverse speed signal)
const int R_EN_PIN = 18; // Right Enable on GPIO 18
const int L_EN_PIN = 19; // Left Enable on GPIO 19

const int ENCODER_A_PIN = 32; // Encoder Channel A[cite: 1]
const int ENCODER_B_PIN = 33; // Encoder Channel B[cite: 1]

const bool INVERT_MOTOR_DIRECTION = true;
// 2. Set INVERT_ENCODER_DIR to 'true' to flip negative velocity readings to positive
const bool INVERT_ENCODER_DIR = true;

// ==========================================
// 2. EXPERIMENT PARAMETERS (ADJUSTABLE DEFAULTS)
// ==========================================
int targetPWM = 50;             // Fixed PWM for both winding (+50) and unwinding (-50)
float pulleyRadius_m = 0.03;    // Pulley radius in meters (e.g., 0.025m = 2.5cm)
float loadMass_kg = 0.05;       // Default load mass in kg (updated via Serial)

const float GRAVITY = 9.81;         // Gravity constant (m/s^2)
const int PWM_FREQ = 1000;          // 1 kHz PWM frequency
const float PULSES_PER_REV = 462.0; // Encoder pulses per output shaft revolution

// ==========================================
// 3. TIMING & DATA LOGGING VARIABLES
// ==========================================
const unsigned long MEASURE_INTERVAL = 40;   // Sample every 40 ms (25 Hz)
const unsigned long TEST_DURATION_MS = 4000;  // Default 5-second wind & unwind duration