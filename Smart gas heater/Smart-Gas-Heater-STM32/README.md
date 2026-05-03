# Smart Gas Heater — Industrial STM32 Upgrade
### STM32G0 (High-Temp SOI) | CO Safety | Remote Control | Polyimide PCB | IEC 60335-2-102

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![MCU](https://img.shields.io/badge/MCU-STM32G031-brightgreen)
![Safety](https://img.shields.io/badge/Safety-IEC%2060335--2--102-red)
![PCB](https://img.shields.io/badge/PCB-Polyimide%20175%C2%B0C-orange)
![Comms](https://img.shields.io/badge/Comms-ESP32%20Wi--Fi%20%7C%20Firebase-blue)

---

## Overview

An **industrial-grade smart gas heater control system** upgraded from the original
Arduino-based design to a production-ready embedded platform. The system addresses
the four core engineering challenges identified in the original project:

| Original Problem | Industrial Solution |
|---|---|
| CO poisoning risk — no automatic shutoff | Dual MQ-7 + electrochemical CO sensor with <200ms response |
| No remote control or monitoring | ESP32 Wi-Fi + Firebase + Android app |
| Controller box too large | Custom polyimide PCB mounted directly on heater chassis |
| 175°C metal surface degrades standard components | STM32G031 SOI MCU + C0G/X7R capacitors + thin-film resistors + AuSn solder |

---

## Safety Architecture — IEC 60335-2-102 Compliance

This system implements safety interlocks in accordance with IEC 60335-2-102
(Safety of household appliances — gas heating appliances).

### Safety Interlock Hierarchy (Priority Order)

```
PRIORITY 1 — HARDWARE INTERLOCK (MCU-independent)
  Thermocouple K-type + MAX31855 + hardware comparator
  If T_surface > 185°C → comparator drives relay OPEN directly
  This fires even if MCU firmware is frozen or crashed

PRIORITY 2 — CO EMERGENCY SHUTDOWN (<200ms)
  MQ-7 electrochemical CO sensor → ADC → firmware ISR
  If CO > 50ppm (OSHA limit) → solenoid valve closed immediately
  Buzzer + LED alert + Firebase notification

PRIORITY 3 — TEMPERATURE SETPOINT CONTROL (PID)
  DHT22 ambient temp → STM32 PID → solenoid PWM duty cycle
  Maintains room temperature at user-set target ±0.5°C

PRIORITY 4 — FLAME SUPERVISION (IEC 60335-2-102 mandatory)
  Flame sensor confirms ignition within 5 seconds of valve open
  If no flame detected → valve closes immediately (gas cut-off)
  3 retry attempts → lockout (manual reset required)
```

---

## Key Specifications

| Parameter | Value |
|---|---|
| MCU | STM32G031K8 (SOI process, -40 to +125°C rated) |
| CO sensor | MQ-7 + MQ-135 dual sensor array |
| CO alarm threshold | 50ppm (OSHA TWA limit) |
| CO emergency trip | 200ppm (IDLH — immediate danger) |
| Temperature sensor | DHT22 (ambient) + K-type thermocouple (surface) |
| Temperature range | 5°C – 30°C setpoint (1°C steps) |
| Gas valve | 24V DC solenoid valve (normally closed — fail-safe) |
| Ignition | Electronic spark igniter (20kV pulse) |
| PCB substrate | Polyimide (PI) — rated to 260°C continuous |
| Solder | AuSn 80/20 — melting point 280°C |
| Passive components | C0G capacitors + thin-film resistors (±25ppm/°C) |
| Communication | ESP32-C3 (Wi-Fi 802.11n, BLE 5.0) |
| Power supply | 24V DC from mains adapter (IEC 60950 compliant) |
| Operating temperature | -20°C to +175°C (PCB surface) |

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     USER INTERFACE                               │
│  Android / iOS App  ←→  Firebase Realtime DB  ←→  Web Dashboard │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Wi-Fi
                               ▼
                    ┌─────────────────┐
                    │  ESP32-C3       │
                    │  Wi-Fi / BLE    │
                    │  UART ↔ STM32   │
                    └────────┬────────┘
                             │ UART
                             ▼
┌────────────────────────────────────────────────────────────────┐
│                  STM32G031K8 (SOI, 125°C rated)                 │
│                                                                  │
│  ADC:  MQ-7 CO sensor (PA0)    DHT22 temp/humid (PA1 One-Wire) │
│        MQ-135 CO2/air (PA2)    Thermocouple via MAX31855 (SPI)  │
│  TIM1: PID control loop (100ms tick)                            │
│  TIM2: Flame sensor timeout (5s ignition window)               │
│  GPIO: Solenoid valve (PB0)   Igniter (PB1)   Buzzer (PB2)    │
│        Flame detect (PA3)     LED alarm (PB3)  LED status (PB4) │
│  USART1: ESP32 telemetry      USART2: Debug                    │
│                                                                  │
│  IWDG: 2s hardware watchdog (critical for gas safety)          │
└───────────────┬──────────────────────────────────────────────────┘
                │
    ┌───────────┴────────────────────────────┐
    │                                        │
┌───▼──────────────────┐     ┌──────────────▼──────────────────┐
│  GAS CONTROL         │     │  HARDWARE SAFETY (MCU bypass)   │
│  24V solenoid valve  │     │  K-type thermocouple            │
│  (normally closed)   │     │  MAX31855 + LM393 comparator    │
│  Electronic igniter  │     │  Direct relay trip at 185°C     │
│  Flame detector      │     │  (fires even if MCU hangs)      │
└──────────────────────┘     └─────────────────────────────────┘
```

---

## High-Temperature PCB Engineering

This is the most critical differentiator from standard Arduino designs.
The heater chassis reaches 175°C — standard FR-4 PCB delaminates at 170°C.

### Material Selections

| Component | Standard (Arduino) | Industrial (this project) | Reason |
|---|---|---|---|
| PCB substrate | FR-4 (Tg 170°C) | Polyimide / Rogers 4350B | Stable to 260°C continuous |
| Solder | SAC305 Pb-free (Tm=217°C) | AuSn 80/20 (Tm=280°C) | Won't reflow at 175°C |
| MCU process | Bulk CMOS (fails >85°C) | SOI CMOS (stable to 125°C) | No latch-up, lower leakage |
| Capacitors | X5R/Y5V (drift 80% at high-T) | C0G (NP0) ±30ppm/°C | Zero capacitance drift |
| Resistors | Thick-film (±200ppm/°C) | Thin-film (±25ppm/°C) | Stable calibration |
| Wire insulation | PVC (melts 105°C) | PTFE/Silicone (260°C) | Survives chassis temp |
| Connector | Standard plastic (melts 100°C) | PTFE/LCP housing | Heat resistant |

---

## Repository Structure

```
Smart-Gas-Heater-STM32/
├── README.md
├── LICENSE
├── .gitignore
├── docs/
│   ├── system_overview.md
│   ├── safety_architecture.md
│   ├── high_temp_pcb_design.md
│   ├── co_sensing_calibration.md
│   ├── pid_temperature_control.md
│   ├── flame_supervision.md
│   └── test_procedure.md
├── hardware/
│   ├── schematics/README.md
│   ├── bom/BOM.csv
│   └── pcb/pcb_design_notes.md
├── firmware/
│   ├── Core/Src/
│   │   ├── main.c
│   │   ├── co_sensor.c
│   │   ├── temperature_control.c
│   │   ├── flame_supervisor.c
│   │   ├── gas_valve.c
│   │   ├── safety_monitor.c
│   │   └── wifi_telemetry.c
│   ├── Core/Inc/
│   │   └── *.h
│   └── README.md
├── software/
│   ├── firebase/firebase_schema.json
│   └── mobile_app/README.md
└── presentation/
    └── Smart_gas_heater.pptx
```

---

## STM32 Pin Assignment

| Pin | Peripheral | Function |
|---|---|---|
| PA0 | ADC_IN0 | MQ-7 CO sensor analog output |
| PA1 | ADC_IN1 | MQ-135 air quality sensor |
| PA2 | ADC_IN2 | Battery/supply voltage monitor |
| PA3 | GPIO in | Flame sensor (IR photodetector) |
| PA4 | SPI1_NSS | MAX31855 thermocouple chip select |
| PA5 | SPI1_SCK | MAX31855 SPI clock |
| PA6 | SPI1_MISO | MAX31855 SPI data |
| PA7 | 1-Wire | DHT22 temp/humidity (bit-bang) |
| PB0 | GPIO out | Solenoid valve driver (via MOSFET) |
| PB1 | GPIO out | Electronic igniter trigger |
| PB2 | GPIO out | Buzzer (CO alarm) |
| PB3 | GPIO out | LED — Alarm (red) |
| PB4 | GPIO out | LED — Status (green) |
| PB5 | GPIO out | LED — Heating (amber) |
| PB6 | USART1_TX | ESP32-C3 telemetry |
| PB7 | USART1_RX | ESP32-C3 telemetry |
| PA9 | USART2_TX | Debug serial |

---

## License

MIT License — see [LICENSE](LICENSE)

## Author

**Adnan Anwar Awan**
Electrical Engineer — Embedded Systems | High-Temperature Electronics | IoT Safety Systems
GitHub: [@AdnanAnwarAwan](https://github.com/AdnanAnwarAwan)
