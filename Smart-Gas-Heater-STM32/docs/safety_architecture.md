# Safety Architecture — IEC 60335-2-102

## Overview

Gas appliances require **fail-safe design** — any single component failure must
result in gas being CUT OFF, never left ON. This is IEC 60335-2-102 §6.2.

Every safety decision in this system follows that rule:
- Solenoid valve is **normally closed** — power failure = gas off
- Hardware comparator trips relay **independently of MCU** — firmware crash = gas off
- Flame supervisor requires **positive confirmation** — no flame = gas off
- IWDG watchdog resets MCU if firmware hangs — and valve closes on reset

---

## Layer 1 — Hardware Over-Temperature Interlock (MCU-independent)

```
K-type thermocouple → MAX31855 → SPI → STM32 (monitoring)
                                   │
                                   └→ Analog comparator (LM393)
                                       Vref = 185°C equivalent voltage
                                       │
                                       └→ Opens relay directly
                                           (bypasses MCU firmware entirely)
```

Why hardware? If firmware hangs in an infinite loop, no software check runs.
A hardware comparator firing a relay directly is the only truly fail-safe approach.

Threshold: **185°C** (10°C above maximum normal operating surface temperature of 175°C)

---

## Layer 2 — CO Emergency Shutdown (<200ms response)

### Dual Sensor Architecture

```
MQ-7  → ADC_IN0 → firmware  (catalytic bead — primary CO sensor)
MQ-135 → ADC_IN1 → firmware  (semiconductor — validates MQ-7 reading)

Logic:
  If MQ-7 > 50ppm AND MQ-135 elevated:
    → CO_ALARM (warning, buzzer, Firebase notification)
  If MQ-7 > 200ppm OR MQ-135 > threshold (independently):
    → CO_EMERGENCY (immediate valve close, lockout, SMS alert)
```

Using two independent sensors prevents single-sensor false trip
(MQ-7 cross-sensitivity to H2, alcohol, etc.) while ensuring
a genuine CO event never goes undetected.

### Response Time

```
ADC sampling rate: 100Hz (10ms per sample)
Moving average window: 2 samples (20ms)
Firmware check ISR: every 20ms
Total CO detection → valve close: < 200ms ✅ (IEC 60335-2-102 requires <5s)
```

---

## Layer 3 — PID Temperature Control

```
DHT22 reads ambient temperature every 2 seconds
STM32 PID computes: error = T_setpoint - T_ambient
Output: gas valve duty cycle (PWM modulation of solenoid)
Integral anti-windup: clamp ±50% duty
Derivative filter: low-pass τ=5s (prevents DHT22 noise triggering derivative spikes)
```

**Safety limit within PID:** Even at maximum integral accumulation,
duty cycle is clamped to 80% — valve is never 100% open continuously.

---

## Layer 4 — Flame Supervision (IEC 60335-2-102 §6.5 mandatory)

```
State machine:

IDLE
  │ user ignite command
  ▼
IGNITING
  │ open solenoid valve (10% duty to limit gas flow)
  │ fire electronic spark igniter (5 pulses @ 1Hz)
  │ start 5-second supervision window (TIM2)
  │
  ├── IF flame sensor HIGH within 5s → RUNNING (normal operation)
  │
  └── IF no flame within 5s → FAILED
          close solenoid valve immediately
          attempt++ 
          IF attempts < 3 → wait 30s → retry IGNITING
          IF attempts >= 3 → LOCKOUT (manual reset only)

RUNNING
  │ flame sensor monitored every 100ms
  └── IF flame lost > 500ms → FAILED (above)

LOCKOUT
  Manual reset required (physical button or app command with PIN)
```

This exactly mirrors the flame supervision sequence required by
IEC 60335-2-102 §6.5.2 for automatic ignition systems.

---

## Layer 5 — IWDG Hardware Watchdog

```
Timeout: 2 seconds
Pet interval: every 500ms in main loop
If not petted → MCU hardware reset → solenoid valve closes (normally closed = safe)

Consequence: if firmware deadlocks or hangs for >2s, gas is automatically cut
```

---

## FMEA Summary (Failure Mode and Effects Analysis)

| Component | Failure Mode | Effect | Mitigation |
|---|---|---|---|
| MCU firmware | Infinite loop / hang | No safety checks | IWDG reset → valve closes |
| MCU hardware | Complete failure | No control | Normally-closed valve closes |
| Solenoid valve | Stuck open | Gas flows uncontrolled | Hardware OT relay cuts power |
| Solenoid valve | Stuck closed | No gas — heater won't start | Operator notification only (safe) |
| MQ-7 sensor | False negative | CO not detected | MQ-135 secondary sensor |
| MQ-7 sensor | False positive | Unnecessary shutdown | Dual-sensor AND logic for alarm |
| Thermocouple | Open circuit | MAX31855 reports fault | Treated as over-temp → trip |
| Flame sensor | Stuck HIGH | Gas flows without flame | Periodic sensor self-test |
| Wi-Fi lost | No remote commands | Heater continues last state | Local safety not affected |
| Power failure | 24V supply lost | Everything de-energises | Normally-closed valve → safe |
