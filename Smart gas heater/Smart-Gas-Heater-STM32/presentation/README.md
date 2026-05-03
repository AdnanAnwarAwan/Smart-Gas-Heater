# Presentation

Original presentation: `Smart_gas_heater.pptx`

## Key Interview Talking Points

### 1. High-Temperature Electronics (Most Unique Aspect)
"The heater chassis reaches 175°C. Standard FR-4 PCB delaminates at 170°C and SAC305
solder reflows at 217°C — which is only 42°C above operating temperature.
I selected polyimide substrate (rated 260°C) and AuSn 80/20 solder (melting point 280°C).
For the MCU, I used an STM32G031 on the SOI process — Silicon-on-Insulator eliminates
latch-up and reduces leakage current by 10× at high temperature compared to bulk CMOS."

### 2. IEC 60335-2-102 Safety Compliance
"Gas appliances must fail safe. Every component failure must result in gas being cut off,
never left on. I implemented a 5-layer safety architecture:
hardware comparator at 185°C (MCU-independent), dual CO sensors with 200ms response,
IEC 60335-2-102 compliant flame supervisor with 3-attempt lockout, IWDG watchdog,
and a normally-closed solenoid valve — power failure = gas off."

### 3. Connection to Picarro
"Picarro's CRDS instruments have the same challenge I solved here — precision analog
sensing in thermally challenging environments. My experience with C0G capacitors,
thin-film resistors, and high-temperature PCB design maps directly to the precision
analog front-ends in optical instruments. The MQ-7 calibration algorithm I implemented
(Rs/R0 ratio with temperature correction) is the same approach used in Picarro's
cavity lock and wavelength calibration systems."

### 4. PID with Safety Integration
"The PID temperature controller is safety-aware — it clamps valve duty to 80% maximum,
forces valve closed when ambient exceeds setpoint+2°C, and integrates with the safety
flag system. All safety events bypass the PID entirely and go directly to valve close."
