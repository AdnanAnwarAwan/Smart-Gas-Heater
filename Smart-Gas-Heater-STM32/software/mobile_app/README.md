# Mobile App (MIT App Inventor / Flutter)

## Features

| Feature | Description |
|---|---|
| Remote ignition | One-tap ignite with safety pre-check confirmation |
| Temperature setpoint | Slider 5–30°C with 0.5°C resolution |
| Real-time dashboard | Live temp, CO ppm, surface temp, flame status |
| CO gauge | Color-coded: green <35ppm, amber 35–50ppm, red >50ppm |
| Alert notifications | Push notification on CO alarm, lockout, flame loss |
| Fault log | Scrollable event log with timestamp and sensor values |
| Lockout reset | PIN-protected reset after emergency lockout |

## Dashboard Layout

```
┌─────────────────────────────────────┐
│  🔥 Smart Gas Heater         [●ON] │
├─────────────────────────────────────┤
│  Room Temp:    20.5°C  Target: 22°C │
│  Surface:     142.3°C  ████░░ OK   │
│  CO Level:      3.2ppm  ●  SAFE    │
│  Humidity:       52%               │
├─────────────────────────────────────┤
│  Temperature Target                 │
│  ─────────────────────● 22°C       │
│  5°C                         30°C  │
├─────────────────────────────────────┤
│  [    IGNITE    ]  [   SHUTDOWN  ] │
├─────────────────────────────────────┤
│  Recent Events                      │
│  10:14 CO alarm 67ppm — valve closed│
│  09:52 Ignition successful          │
└─────────────────────────────────────┘
```

## Firebase Integration

App reads from `realtime/` node every 1 second.
Commands written to `commands/pending` node.
ESP32-C3 polls `commands/pending` and sends to STM32 via UART.
STM32 sends reply → ESP32 clears `commands/pending`.
