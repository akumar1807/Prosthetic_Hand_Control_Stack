# Prototype 1 – Two-Finger Underactuated Hand

## Abstract

This project presents the development of a **two-finger underactuated robotic hand prototype**. The design uses a closed-loop current feedback control system for a DC motor to actuate a compliant mechanism, enabling smooth finger flexion and extension.

---

## Hardware Used for the Control Loop

### 1. ESP32 Microcontroller

The ESP32 collects readings of the current drawn by the DC motor through a current and power monitor and outputs control commands to the motor driver.

### 2. BTS7960 H-Bridge Motor Driver

The BTS7960 motor driver is directly connected to the ESP32 and the DC motor. It receives control commands from the ESP32 and drives the motor accordingly.

* [BTS7960 Motor Driver](https://robocraze.com/products/bts7960b-43a-h-bridge-motor-driver?variant=40193735065753&country=IN&currency=INR&utm_source)
* [BTS7960 Datasheet](https://www.handsontec.com/dataspecs/module/BTS7960%20Motor%20Driver.pdf)

### 3. INA226 Voltage, Current and Power Monitor Module

The INA226 is used to measure the current drawn by the DC motor and acts as the sensor in the feedback control loop.

* [INA226 Module](https://robu.in/product/ina226-voltage-current-and-power-monitor-module/)
* [INA226 Datasheet](https://www.ti.com/lit/ds/symlink/ina226.pdf)

### 4. Solid Shaft JGY370 DC 12 V Worm Reduction Motor

The JGY370 DC motor drives the complete finger grasping and compliance mechanism. It operates at **12 V DC** and can draw nearly **2 A** under maximum shaft load.

* [JGY370 Worm Reduction Motor](https://robu.in/product/solid-shaft-jgy370-dc12v-210rpm-min-high-torque-self-locking-strong-wheel-tturbine-worm-reduction-motor/)
* [Additional Motor Details](https://ielectrony.com/en/product/solid-shaft-jgy370-dc12v-210rpm-min-high-torque-self-locking-strong-wheel-tturbine-worm-reduction-motor/)

### 5. Mechanical Setup

The complete mechanism is mounted on metal guide rails. The fingers are **3D-printed using TPU** to provide compliance during grasping.

---

## Control Loop

The opening and closing mechanism uses the motor current as feedback.

The INA226 measures the current drawn by the motor under both no-load and loaded conditions. When an external force acts on the mechanism connected to the motor shaft, the motor must produce additional torque and therefore draws more current.

Using this principle, four operating states were defined:

| State                  | Approximate Motor Current |
| ---------------------- | ------------------------: |
| **IDLE**               |                  ~0.056 A |
| **CLOSING**            |                   ~0.08 A |
| **OPENING**            |                   ~0.08 A |
| **HOLDING / GRASPING** |                ~0.8–1.5 A |

The ESP32 reads the current measured by the INA226 and sends commands to the motor controller so that the system can switch between these four states.

---

## Motor Control with Vision-Based Commands

For the **Delsys Executive Chair demonstration**, the control algorithm was revised so that finger movement could be controlled using visual input from **MediaPipe**.

A high-level MediaPipe script detects the user's hand gesture and sends commands to the low-level ESP32 microcontroller over **UDP**. The ESP32 then commands the motor to rotate in the appropriate direction to open or close the hand.

### Control Flow

```text
User Hand Gesture
       ↓
Webcam / Camera
       ↓
MediaPipe Hand Tracking
       ↓
Gesture Detection
       ↓
OPEN / CLOSE Command
       ↓
UDP over Wi-Fi
       ↓
ESP32
       ↓
BTS7960 Motor Driver
       ↓
JGY370 DC Motor
       ↓
Two-Finger Prosthetic Hand
```

---

## Using MediaPipe to Capture User Gestures

[MediaPipe](https://ai.google.dev/edge/mediapipe) is an open-source framework developed by Google for processing live and streaming media. In this project, it is used to track the user's hand and place virtual landmarks at specific locations on the fingers.

Four landmarks are primarily used:

| Landmark           | MediaPipe Index |
| ------------------ | --------------: |
| `THUMB_MCP`        |               2 |
| `THUMB_TIP`        |               4 |
| `INDEX_FINGER_MCP` |               5 |
| `INDEX_FINGER_TIP` |               8 |

The 2D distance between landmarks **4** and **8** is calculated and compared against a minimum threshold to determine whether the prosthetic hand should open or close.

To create a softer and more reliable threshold, the angle formed using landmarks **(4, 2)** and **(8, 5)** is also considered when determining the **OPEN** command.

---

## Sending Commands to the ESP32 Using UDP

**User Datagram Protocol (UDP)** is a connectionless transport-layer protocol designed for fast, low-latency communication. This makes it suitable for sending real-time hand-state commands from the vision system to the ESP32.

### UDP Code

```python
import socket

# Requires the IP address of the ESP32
ESP32_IP = "192.168.0.121"
UDP_PORT = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Send command via Wi-Fi only if the state has changed
if new_state != current_hand_state:
    try:
        sock.sendto(new_state.encode(), (ESP32_IP, UDP_PORT))
        print(f"Sent command to ESP32: {new_state}")
        current_hand_state = new_state

    except Exception as e:
        print(f"Failed to send network data: {e}")
```

Sending data only when the detected state changes prevents unnecessary UDP traffic and repeated motor commands.

---

## ESP32 Motor Control

For the vision-controlled demonstration, the current-sensor feedback behavior was replaced with a time-based motor-control strategy.

When a new `OPEN` or `CLOSE` command is received, the ESP32 runs the motor for a fixed duration.

The motor speed follows a **trapezoidal ramp profile**, consisting conceptually of:

```text
Motor Speed
    ^
    |           __________
    |          /          \
    |         /            \
    |________/              \________
    |
    +---------------------------------> Time
         Ramp Up   Constant   Ramp Down
```

This allows the motor to accelerate smoothly, operate at its commanded speed, and decelerate smoothly instead of changing speed instantaneously.

---

## Repository

More details and implementation code are available in this repository:

[**Prosthetic Hand Control Stack**](https://github.com/akumar1807/Prosthetic_Hand_Control_Stack/tree/main)

For collaboration requests, please contact the project author.
