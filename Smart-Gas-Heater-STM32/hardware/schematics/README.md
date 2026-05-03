# Schematics

## CO Sensor Front-End (per sensor)

```
MQ-7 pin A (+5V heater) → controlled by PWM GPIO (MQ7_HEAT_PIN)
MQ-7 pin B (GND)        → GND
MQ-7 pin C (output)     → 10kΩ load to GND → ADC input

Signal path:
  MQ-7 output → SMBJ5.0A TVS (clamp) → 10nF C0G filter → STM32 PA0 ADC_IN0
```

## Hardware Over-Temperature Interlock (MCU-independent)

```
K-type thermocouple → MAX31855 → SPI → STM32 (software monitoring)
                                   │
Thermocouple voltage → LM393 comparator
  V+ = thermocouple voltage scaled to match 185°C equivalent
  V- = precision voltage reference (TL431) set to 185°C threshold

LM393 output → drives base of NPN transistor → opens relay
  Relay in series with 24V solenoid valve power
  If T > 185°C: relay opens → valve power cut → gas off
  This fires regardless of MCU firmware state
```

## Solenoid Valve Drive Circuit

```
STM32 PB0 (TIM3_CH1 PWM)
    │
   IRFR9024N P-channel MOSFET (Vgs threshold < 2V — drives from 3.3V logic)
    │
   Solenoid valve coil (24V, ~100Ω)
    │
   1N4007 flyback diode (mandatory — valve back-EMF up to 100V peak)
    │
   GND
```

## Complete Pin Map

| STM32G031 Pin | Function | Connected to |
|---|---|---|
| PA0 | ADC_IN0 | MQ-7 output (via 10kΩ load + TVS) |
| PA1 | ADC_IN1 | MQ-135 output |
| PA2 | ADC_IN2 | 24V supply monitor (100k/10k divider) |
| PA3 | GPIO in | Flame sensor (IR photodetector output) |
| PA4 | SPI1_NSS | MAX31855 CS (active low) |
| PA5 | SPI1_SCK | MAX31855 clock |
| PA6 | SPI1_MISO | MAX31855 data out |
| PA7 | GPIO (1-Wire) | DHT22 data pin (open-drain) |
| PB0 | TIM3_CH3 | Solenoid valve PWM (via MOSFET) |
| PB1 | GPIO out | Electronic igniter trigger |
| PB2 | GPIO out | Buzzer (5V active piezo) |
| PB3 | GPIO out | Alarm LED red |
| PB4 | GPIO out | Status LED green |
| PB5 | GPIO out | Heating LED amber |
| PB6 | USART1_TX | ESP32-C3 RX |
| PB7 | USART1_RX | ESP32-C3 TX |
| PB6 | GPIO out | MQ-7 heater control (HIGH HEAT = 5V, LOW HEAT = PWM) |
| PA9 | USART2_TX | Debug serial 115200 |
| NRST | Reset | Physical reset button |
| BOOT0 | Bootloader | Programming mode selection |
