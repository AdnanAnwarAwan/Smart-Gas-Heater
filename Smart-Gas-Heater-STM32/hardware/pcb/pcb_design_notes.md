# PCB Design Notes — High-Temperature Polyimide Board

## Board Specifications
- Substrate: Polyimide (Kapton) — rated 260°C continuous
- Solder: AuSn 80/20 paste — melting point 280°C
- Copper weight: 2oz (derating for 175°C ambient)
- Layers: 4 (Signal / GND / PWR / Signal_Bottom)
- Finish: ENIG (electroless nickel/gold) — survives thermal cycling
- Assembly: N2 reflow atmosphere required for AuSn
- No through-hole components: all SMD only (PTH barrel cracking risk)
- Board dimensions: 60 × 40mm (fits within heater chassis cavity)

## Mounting
- Board mounts directly on heater metal chassis
- Interface: ceramic thermal interface sheet (Bergquist GP3000 equivalent, 260°C rated)
- Standoffs: ceramic M3 standoffs (never nylon or plastic)
- Board attachment: M3 stainless steel screws

## Critical Layout Zones

### Zone 1 — Sensor Input (left side)
```
MQ-7 and MQ-135 connector → 10kΩ load resistors → ADC filter → STM32 ADC pins
TVS clamp on each ADC input trace
Keep sensor traces away from solenoid valve switching traces
```

### Zone 2 — STM32 + ESP32 (centre)
```
STM32G031 with crystal and all decoupling caps (C0G only, placed within 2mm of pins)
ESP32-C3 module with RF keep-out zone (no copper 5mm from antenna)
SPI traces to MAX31855 — matched length
```

### Zone 3 — Power Output (right side)
```
Solenoid valve MOSFET + relay — high current, keep away from analog
Flyback diode across solenoid valve coil — mandatory
Thick traces: 2mm minimum for valve drive
```

## Thermal Design
- Hottest components: solenoid MOSFET, relay coil
- Thermal vias under MOSFET: 0.3mm drill, 9 vias → chassis copper
- Component power dissipation must be recalculated for 175°C ambient derating

## Ground Strategy
- Single GND plane on L2 — never split for this board size
- Analog GND (sensor returns) meets digital GND at single point near ADC VREF
- Valve/relay return current: thick trace direct to power entry, not through logic area
