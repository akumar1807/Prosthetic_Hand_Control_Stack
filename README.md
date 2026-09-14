# Underactuated Prosthetic Hand Control Stack

## Control of a Two-Finger Underactuated Prosthetic Hand with Tactile Sensing

This project focuses on the development, implementation, and experimental validation of a control system for a two-finger underactuated prosthetic hand incorporating motor position sensing, Series Elastic Actuation (SEA), compliant finger joints, and tactile/force sensing.

> **Prototype 1:** For the Prototype 1 codebase, switch to the `prototype_one` branch.

---

## 1. Project Objective

To develop, implement, and experimentally validate a control system for a two-finger underactuated prosthetic hand incorporating:

* Motor position sensing
* Series Elastic Actuation (SEA)
* Compliant finger joints
* Tactile/force sensing

---

## 2. Project Scope

The project will focus primarily on the **control system** of the two-finger underactuated prosthetic hand.

The work will progress through the following stages:

1. PMDC motor characterization
2. Motor position control
3. SEA characterization and force control
4. Contact detection
5. Complete grasping experiments
6. Experimental validation

---

## 3. Project Responsibilities

### 3.1 ESP32-Based Control System

* Develop the ESP32-based control architecture.
* Interface the PMDC motor, motor driver, and sensors with the ESP32.
* Develop real-time sensor acquisition.
* Develop motor-control algorithms.
* Implement safety limits and fault-handling mechanisms.

### 3.2 PMDC Motor Characterization

Characterize the PMDC motor experimentally to determine its operating characteristics.

#### Parameters to be Obtained

* No-load speed
* Motor current
* Speed–current relationship
* Torque–current relationship
* Stall current and torque
* Motor response time

The required sensors for motor characterization will also be identified and integrated.

### 3.3 Motor Position Control

* Integrate the existing Hall-effect sensor for motor position measurement.
* Develop motor position measurement using the Hall sensor.
* Implement closed-loop position control.
* Develop and tune a PID controller.
* Evaluate position tracking and dynamic response.

---

## 4. Integration of Compliance

The complete hand consists of multiple compliant components:

**PMDC Motor → Transmission → SEA → Compliant Finger Joints → Contact**

Therefore, the control system must consider the compliance of both the **SEA** and the **finger joints**.

### Tasks

* Integrate the Hall-effect sensor for motor position.
* Integrate the Hall-effect sensor for SEA displacement.
* Characterize the compliant element using the SEA sensor.
* Determine the relationship between SEA displacement and applied force.
* Determine the stiffness of the compliant element.
* Develop a mathematical model of the compliant system.

---

## 5. Mathematical Modelling

Develop mathematical models describing the relationship between actuator motion, compliance, and hand motion.

The following relationships should be investigated:

1. **Motor position → SEA displacement**
2. **SEA displacement → Force**
3. **Motor position → Finger position**
4. **Motor torque → Finger force**
5. **Finger motion → Contact force**

The developed models will be used for controller design and experimental validation.

---

## 6. Control Architecture

The control system will be developed in multiple stages.

### Stage 1: Motor Position Control

```text
Desired Motor Position
          ↓
    PID Controller
          ↓
     Motor Driver
          ↓
      PMDC Motor
          ↓
    Motor Position
     Hall Sensor
          └─────────────── Feedback ───────────────┘
```

**Feedback:**

* Motor Hall-effect sensor

### Stage 2: SEA-Based Control

```text
Desired Force
      ↓
Force Controller
      ↓
  PMDC Motor
      ↓
      SEA
      ↓
   Finger
      ↑
      │
Force estimated from
SEA deformation
```

**Feedback:**

* Motor position
* SEA displacement
* Force estimated from SEA deformation

### Stage 3: Complete Hand Control

```text
Desired Grasp
      ↓
  Controller
      ↓
     Motor
      ↓
      SEA
      ↓
Compliant Fingers
      ↓
    Object
      ↑
      │
Tactile / Contact Sensing
```

**Feedback:**

* Motor position
* SEA displacement/force
* Tactile/contact sensing

---

## 7. Contact Detection

Develop an algorithm to detect when the prosthetic fingers make contact with an object.

The controller should distinguish between:

* **Free movement**
* **Initial object contact**
* **Grasping**
* **Stable grasp**

The contact-detection algorithm should use the available **SEA and/or tactile sensing information**.

---

## 8. Force Control Through SEA

Develop closed-loop force control using the **Series Elastic Actuator (SEA)**.

### Tasks

* Estimate grasping force from SEA deformation.
* Establish the SEA displacement–force relationship.
* Implement a force-control algorithm.
* Track a desired grasping force.
* Evaluate force-control accuracy and response.
* Perform grasping experiments with different objects.

---

## 9. Safety Control

Implement safety mechanisms to protect the motor, mechanism, and test objects.

The controller should include:

* Maximum motor-current limit
* Maximum motor-position limit
* Maximum SEA displacement
* Maximum allowable grasping force
* Sensor-failure detection
* Motor stall detection
* Emergency motor shutdown

---

## 10. Experimental Identification

Experimentally identify the parameters required for modelling and controller development.

### Parameters to be Identified

* Motor characteristics
* Motor position response
* Transmission characteristics
* SEA stiffness
* SEA displacement–force relationship
* Compliant joint stiffness
* System response time
* Friction and backlash, if significant

---

## 11. Validation Experiments

The developed control system will be experimentally validated through the following tests:

| Experiment              | Objective                                      |
| ----------------------- | ---------------------------------------------- |
| Motor characterization  | Determine motor parameters                     |
| Motor position tracking | Validate PID controller                        |
| SEA characterization    | Determine stiffness and force relationship     |
| Contact detection       | Evaluate contact-detection algorithm           |
| Force tracking          | Evaluate force-control performance             |
| Grasping experiments    | Test grasping of different objects             |
| Safety tests            | Verify current, force, and displacement limits |

---

## 12. Final System Implementation

The final system will integrate:

* ESP32
* PMDC motor
* Motor driver
* Motor Hall-effect sensor
* SEA Hall-effect sensor
* Tactile/force sensing
* PID/force-control algorithms
* Safety-control system

### Final Control Sequence

```text
Motor Position Control
          ↓
SEA Characterization
          ↓
Contact Detection
          ↓
Force Control
          ↓
Safe Grasping
          ↓
Experimental Validation
```

---

## 13. Expected Outcomes

At the completion of the project, the student should be able to:

1. Characterize the PMDC motor experimentally.
2. Interface the motor and sensors with an ESP32.
3. Implement closed-loop motor position control.
4. Characterize the SEA and compliant elements.
5. Develop mathematical models of the actuator–SEA–finger system.
6. Detect contact using tactile/SEA sensing.
7. Implement force control through the SEA.
8. Implement basic safety and fault-control mechanisms.
9. Demonstrate controlled grasping using the two-finger prosthetic hand.
10. Validate the complete control system experimentally.

---

## Repository Structure

The repository contains the control-system development for the two-finger underactuated prosthetic hand.

For **Prototype 1**, switch to:

```bash
git checkout prototype_one
```

or:

```bash
git switch prototype_one
```
