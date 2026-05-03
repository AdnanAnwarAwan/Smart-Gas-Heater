# High-Temperature PCB Design

## The Core Challenge

The gas heater metal chassis reaches **175°C** during normal operation.
Standard electronics are designed for 0–70°C (commercial) or -40–85°C (industrial).
Every standard material choice fails at 175°C.

## PCB Substrate Selection

### Why FR-4 Fails

```
FR-4 glass transition temperature (Tg): 130–170°C (standard)
At Tg: resin softens → PCB warps → traces crack → delamination
At 175°C (chassis temperature): FR-4 is ABOVE Tg → board fails mechanically

Timeline of FR-4 failure at 175°C:
  0–10 min:   Board softens, small dimensional change
  10–30 min:  Visible warping, trace stress
  30–60 min:  Possible delamination between copper and substrate
  >60 min:    Irreversible damage, likely electrical failure
```

### Polyimide (Kapton) Solution

```
Polyimide (PI) substrate:
  Glass transition temperature: >250°C
  Continuous use temperature:   260°C
  Short-term peak:              400°C
  Dielectric constant:          3.5 (stable vs temperature)
  CTE (Z-axis):                 ~50ppm/°C (matched with through-holes)
  Cost premium vs FR-4:         ~4×

Rogers 4350B (alternative for RF sections):
  Tg:                          >280°C
  Dk:                          3.66 (±0.05 vs temperature)
  Better dimensional stability than polyimide for fine-pitch components
```

## Solder Selection

### Why SAC305 Lead-Free Solder Fails

```
SAC305 (96.5% Sn / 3% Ag / 0.5% Cu):
  Melting point: 217°C
  At 175°C: 81% of melting point — thermal fatigue accelerates exponentially
  Expected joint life at 175°C: <1000 thermal cycles
  Failure mode: joint fatigue cracking (solder creep)
```

### AuSn 80/20 Solution

```
AuSn (80% Au / 20% Sn):
  Melting point:           280°C
  Operating margin at 175°C: 105°C below melting → negligible creep
  Thermal fatigue life:    >100,000 cycles at 175°C
  CTE:                     16 ppm/°C (matches copper well)
  Drawback:                Very expensive (~$500/troy oz gold content)
  Process:                 Requires N2 reflow atmosphere — no flux (contamination risk)

Alternative: High-lead solder PbSn (85/15)
  Melting point: 300°C
  Lower cost than AuSn but uses lead (RoHS exemption required for high-temp)
```

## Component Selection for 175°C

### Capacitors

```
X5R/Y5V MLCC (standard choice):
  At 85°C:  capacitance drops 15%
  At 125°C: capacitance drops 50%
  At 175°C: capacitance drops >80% — functionally useless

C0G (NP0) MLCC (industrial choice):
  Temperature coefficient: ±30 ppm/°C (essentially flat)
  At 175°C: capacitance changes <0.6% from 25°C value ✅
  Cost premium: ~3×
  Availability limitation: C0G not available >10µF typically
  Solution: use C0G for critical timing/filtering; X7R for bulk bypass

X7R MLCC (acceptable compromise):
  Rated to 125°C (minimum) or 150°C (extended)
  At 150°C: ±15% capacitance shift (acceptable for bypass)
  NOT suitable for: timing circuits, ADC references, filter components
```

### Resistors

```
Thick-film resistors (standard):
  TCR: ±100 to ±200 ppm/°C
  At 175°C: resistance shifts 15–30% → ADC readings uncalibrated

Thin-film resistors (industrial choice):
  TCR: ±10 to ±25 ppm/°C
  At 175°C: resistance shifts <1.5% ✅
  Packages: 0402, 0603 — same as thick-film
  Cost: ~5× thick-film
  Required for: ADC input dividers, sensor bias, PID reference

Metal strip resistors for current sensing:
  TCR: <5 ppm/°C
  Use for: shunt resistors in power monitoring
```

### Microcontroller — SOI Process

```
Standard bulk CMOS (Arduino ATmega, standard STM32):
  Rated to: 85°C (commercial) or 105°C (industrial)
  At 175°C:
    - Leakage current increases exponentially → power consumption spikes
    - Threshold voltage shifts → logic levels unreliable
    - Latch-up risk increases (parasitic SCR triggered by leakage)
    - Expected MTBF: <100 hours

STM32G031 (Silicon-on-Insulator process):
  SOI eliminates the bulk substrate conduction path
  - Latch-up immune (no parasitic SCR possible)
  - Leakage current 10× lower at high temperature
  - Rated: -40°C to +125°C (junction temperature)
  - At 175°C chassis: junction T = chassis + self-heating ≈ 180°C
    → marginal but acceptable with good thermal management

Best choice for extreme temperature:
  Infineon XMC1000 (to 150°C) or TI SimpleLink MSP432 (to 125°C)
  For >150°C junction: Texas Instruments TMDSICE3359 (AEC-Q100 Grade 0)
```

## PCB Layout for High Temperature

### Thermal Management Rules

```
1. Via stitching under MCU: 0.3mm drill, 8+ vias → spreads heat to inner copper
2. Board standoffs: ceramic or PTFE (not nylon — melts at 80°C)
3. Component placement: keep temperature-sensitive parts (MCU, sensors) away
   from chassis hot-spots — use thermal simulation to identify cool zones
4. Copper fills: heavier copper (2oz minimum) for better heat spreading
5. No through-hole components: thermal stress cracks PTH barrels
   → use 100% SMD with AuSn solder
6. Board attachment to chassis: ceramic thermal interface material
   (standard thermal pads melt at 120°C)
```

### Trace Width for Elevated Temperature

```
At 175°C ambient, copper resistivity increases 75%:
  Standard 1mm/amp rule → must derate to 0.6mm/amp
  All power traces: recalculate with 175°C ambient derating
  Current carrying capacity: use IPC-2152 calculator with 175°C ambient
```
