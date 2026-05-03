# Flame Supervision — IEC 60335-2-102 §6.5

## Ignition Sequence State Machine

```
States:  IDLE → IGNITING → SUPERVISING → RUNNING → (FAILED → LOCKOUT)

IDLE:
  Heater off, valve closed, igniter off
  Awaiting user command (app or local button)

IGNITING (max 5 seconds):
  1. Open solenoid valve to 15% PWM (controlled gas flow)
  2. Fire electronic spark igniter: 5 pulses at 1Hz (20kV each)
  3. Monitor flame sensor every 100ms
  4. TIM2 starts 5-second countdown

  → Flame detected: transition to SUPERVISING
  → 5s elapsed, no flame: transition to FAILED

SUPERVISING (500ms confirmation window):
  Flame must stay HIGH for 500ms continuously
  Prevents brief ignition spark reflections triggering false positive

  → Flame stable 500ms: transition to RUNNING (normal operation)
  → Flame lost during 500ms: transition to FAILED

RUNNING:
  PID temperature control active
  Flame sensor checked every 100ms
  CO sensor active

  → User shutdown command: transition to IDLE
  → Flame lost >500ms: transition to FAILED
  → CO alarm: transition to IDLE (valve closed)
  → Over-temperature: transition to IDLE (hardware interlock)

FAILED:
  Close solenoid valve immediately
  Stop igniter
  Log failure event + timestamp + attempt number
  attempt_count++

  → IF attempt_count < 3: wait 30s → transition to IGNITING
  → IF attempt_count >= 3: transition to LOCKOUT

LOCKOUT:
  Valve closed, igniter off, alarm LED steady red
  Cannot be reset remotely (Firebase command rejected)
  Requires physical button press on unit + PIN via app
  Logs LOCKOUT event to Firebase
```

## Electronic Ignition System

```
STM32 PB1 → MOSFET (IRFZ44N) → ignition transformer primary
                                  Secondary: 20kV spark
                                  Pulse: 5ms ON, 200ms OFF
                                  5 pulses per ignition attempt
```

---

# Test & Validation Procedure

## Equipment Required
- Calibrated CO gas source (50ppm and 200ppm certified CO cylinders)
- Thermometer (calibrated, ±0.1°C)
- Oscilloscope
- Smartphone with app installed
- Multimeter
- Thermocouple calibrator

---

## Step 1 — Power and Logic
- [ ] 24V supply rail: **23–25V** ✅
- [ ] 5V logic rail: **4.9–5.1V** ✅
- [ ] 3.3V MCU rail: **3.28–3.32V** ✅
- [ ] STM32 boots — USART shows startup banner ✅
- [ ] ESP32 connects to Wi-Fi — Firebase dashboard loads ✅

## Step 2 — CO Sensor Calibration
- [ ] MQ-7 warm-up complete (5 minutes) ✅
- [ ] Clean air reading → R0 stored in flash ✅
- [ ] Apply 50ppm CO source → dashboard shows **45–55ppm** ✅
- [ ] Apply 200ppm CO source → dashboard shows **180–220ppm** ✅

## Step 3 — CO Safety Interlock
- [ ] Apply 50ppm CO → alarm triggers, buzzer sounds ✅
- [ ] Valve closes within **200ms** of 50ppm threshold ✅
- [ ] Firebase notification received within 5s ✅
- [ ] Apply 200ppm CO → LOCKOUT state, cannot remote restart ✅

## Step 4 — Temperature Sensing
- [ ] DHT22 reads ambient: ±0.5°C vs reference thermometer ✅
- [ ] MAX31855 reads surface: ±1°C vs thermocouple calibrator ✅
- [ ] Surface temp > 185°C → hardware relay opens ✅ (test with calibrator)

## Step 5 — Flame Supervision
- [ ] Ignition command → valve opens → spark fires ✅
- [ ] Flame detected within 3s → RUNNING state ✅
- [ ] Block flame sensor → flame loss detected within **500ms** → valve closes ✅
- [ ] Remove ignition gas source → 3 failed attempts → LOCKOUT ✅
- [ ] Lockout: remote restart rejected, requires physical + app PIN ✅

## Step 6 — PID Temperature Control
- [ ] Set 22°C target, room at 18°C → valve opens, temperature rises ✅
- [ ] Steady-state: temperature within **±0.5°C** of setpoint ✅
- [ ] Set 25°C target → valve opens proportionally, no overshoot >1°C ✅
- [ ] Set target below current temp → valve closes ✅

## Step 7 — Remote Control (Wi-Fi)
- [ ] App: ignite command → heater starts ✅
- [ ] App: set 24°C → heater adjusts ✅
- [ ] App: shutdown → valve closes within 2s ✅
- [ ] App: real-time temp, CO, flame status update < 1s ✅

## Step 8 — Hardware Watchdog
- [ ] Suspend main loop in debugger > 2s → MCU resets → valve closes ✅

## Step 9 — Power Failure
- [ ] Remove 24V supply while valve open → valve closes (normally closed) ✅
- [ ] Restore power → STM32 boots to IDLE (does not auto-ignite) ✅

## Step 10 — High Temperature Soak
- [ ] Run at full operation for 4 hours at 45°C ambient
- [ ] All readings stable, no component failures ✅
- [ ] CO calibration within ±5ppm after soak vs before ✅
