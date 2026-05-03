# PID Temperature Control

## Overview

The system maintains the room at a user-set temperature by modulating
the gas flow through the solenoid valve using PWM duty cycle control.

```
User setpoint (°C) → PID controller → Valve PWM duty → Gas flow → Room temp
         ↑                                                              │
         └──────────────── DHT22 feedback (every 2s) ←─────────────────┘
```

## PID Parameters

```
Kp = 8.0    (proportional — fast initial response)
Ki = 0.1    (integral — eliminates steady-state error over ~10 minutes)
Kd = 2.0    (derivative — dampens temperature overshoot)

Sample time: 2000ms (DHT22 minimum update rate is 2s)
Output:      0–80% valve duty (never 100% — safety margin)
Anti-windup: integral clamped to ±50% duty equivalent
```

## Why Valve PWM Instead of On/Off?

On/Off (bang-bang) control:
- Valve slams fully open then fully closed
- Temperature oscillates ±2–5°C around setpoint
- Repeated solenoid cycling reduces valve lifetime
- Gas pressure pulses cause audible "thumping"

PWM modulation:
- Valve opens proportionally to demand
- Temperature maintained ±0.5°C around setpoint
- Smooth, quiet operation
- Solenoid operated at 10–20Hz PWM (below audible resonance)

## Derivative Kick Prevention

When user changes setpoint, a step change in error causes a large
derivative spike (D-kick) that can slam the valve fully open.

Solution: derivative on measurement (not error):

```c
float derivative = -(measured - pid.prev_measured) / dt;
//                   ↑ negate because higher temp = lower error
pid.prev_measured = measured;
```

## Safety Limits Integrated into PID

```c
/* Hard limits applied after PID calculation */
duty = CLAMP(duty, 0.0f, 80.0f);      /* Never >80% valve open */

/* If ambient temp already above setpoint+2°C → force valve closed */
if (measured > (setpoint + 2.0f)) {
    duty = 0.0f;
    pid.integral = 0.0f;    /* Reset integrator to prevent windup */
}

/* Valve always closed if any safety flag is set */
if (safety_flags != SAFETY_OK) {
    duty = 0.0f;
}
```

## Setpoint Limits

- Minimum setpoint: **5°C** (prevents pipe freezing mode)
- Maximum setpoint: **30°C** (above this, CO risk increases from poor ventilation)
- Default setpoint: **20°C**
- Step resolution: **0.5°C** (app slider)
