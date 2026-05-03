# Firmware

## Build Requirements
- STM32CubeIDE 1.13+ or VS Code + arm-none-eabi-gcc
- STM32CubeG0 HAL library
- ST-Link V2 programmer (SWD)
- Board: STM32G031K8T6 (LQFP32)

## File Structure

| File | Purpose |
|---|---|
| `main.c` | System orchestration, command handler, state machine |
| `co_sensor.c` | MQ-7 heating cycle, ADC reading, R0 calibration, ppm calculation |
| `temperature_control.c` | PID with anti-windup, derivative on measurement |
| `flame_supervisor.c` | IEC 60335-2-102 §6.5 ignition + flame supervision state machine |
| `gas_valve.c` | Solenoid PWM, normally-closed fail-safe |
| `safety_monitor.c` | CO/temperature safety flags, lockout PIN validation |
| `wifi_telemetry.c` | ESP32-C3 UART protocol, telemetry packets, alert messages |

## Safety-Critical Constants (do not change without engineering review)

| Constant | Value | File | Why |
|---|---|---|---|
| CO_ALARM_PPM | 50 | safety_monitor.c | OSHA PEL — valve close |
| CO_EMERGENCY_PPM | 200 | safety_monitor.c | NIOSH hazard — lockout |
| SURFACE_TRIP_TEMP | 185°C | safety_monitor.c | 10°C above max normal |
| IGNITION_WINDOW_MS | 5000 | flame_supervisor.c | IEC 60335-2-102 §6.5 |
| MAX_IGNITE_ATTEMPTS | 3 | main.c | IEC 60335-2-102 lockout |
| VALVE_MAX_DUTY | 80% | gas_valve.c | Never fully open |
| IWDG_TIMEOUT | 2000ms | MX_IWDG_Init | Firmware hang safety |

## MQ-7 Calibration Procedure

1. Take sensor outdoors (clean air) at 20°C, ~65% humidity
2. Power on system, wait 5 minutes (warm-up)
3. Send command `CALIBRATE` via app or debug UART
4. System measures Rs and calculates R0 = Rs/3.0
5. R0 stored in STM32 flash — survives power cycles
6. Recalibrate annually or after sensor replacement

## Debug UART Output Example (USART2, 115200 baud)

```
=== Smart Gas Heater STM32G031 ===
Safety: IEC 60335-2-102 | CO thresholds: 35/50/200ppm
PCB: Polyimide | Solder: AuSn | MCU: SOI process
State: IDLE — awaiting ignition command

MQ-7 R0 loaded: 8543 Ω
MQ-7: HIGH HEAT phase (60s warm-up)
MQ-7: LOW HEAT phase — reading active
CMD: IGNITE
Flame supervisor: IGNITING
Spark 1/5
Spark 2/5
Flame detected — supervising
Ignition successful — RUNNING
PID: T=19.8 SP=22.0 err=2.2 duty=48.3%
PID: T=20.5 SP=22.0 err=1.5 duty=31.2%
PID: T=21.8 SP=22.0 err=0.2 duty=4.1%
```
