# System Overview

## Problem Statement (from original project)

1. **CO poisoning** — traditional heaters have no automatic CO detection or shutoff
2. **Unattended operation** — no remote monitoring or temperature limits
3. **Bulky controller** — external Arduino box impractical for a household appliance
4. **175°C environment** — standard electronics fail at chassis temperature

## Industrial Solution Summary

### Hardware Upgrades

| Original | Upgraded | Why |
|---|---|---|
| Arduino Uno (ATmega328) | STM32G031 (SOI process) | Survives 125°C junction temperature |
| Standard PCB (FR-4) | Polyimide substrate | Survives 260°C continuous |
| Lead-free solder (Tm=217°C) | AuSn 80/20 (Tm=280°C) | Won't reflow at 175°C chassis |
| Standard capacitors (X5R) | C0G/NP0 capacitors | Zero capacitance drift with temperature |
| Single MQ-7 sensor | Dual MQ-7 + MQ-135 | Redundant CO detection, cross-validation |
| No temperature monitoring | K-type thermocouple + MAX31855 | Hardware over-temperature interlock |
| No ignition feedback | IR flame sensor (IEC 60335-2-102 §6.5) | Mandatory flame supervision |
| Bluetooth only | ESP32-C3 Wi-Fi + BLE | Longer range, Firebase cloud integration |

### Firmware Upgrades

| Original | Upgraded |
|---|---|
| Simple threshold on/off | PID temperature control (±0.5°C) |
| Single CO check | Dual sensor validation + moving average filter |
| No ignition sequence | 5-state flame supervisor with 3-attempt lockout |
| No watchdog | IWDG 2s hardware watchdog |
| No fault logging | Firebase fault log with timestamp + sensor values |
| No calibration | MQ-7 R0 calibration stored in flash, temp/humidity corrected |
