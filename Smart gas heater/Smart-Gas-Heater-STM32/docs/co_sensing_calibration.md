# CO Sensing & Calibration

## Sensor Selection — Dual Architecture

### MQ-7 (Primary — Catalytic Bead)

```
Principle:  Catalytic oxidation of CO on heated bead
Sensitivity: 20–2000 ppm CO
Selectivity: good CO selectivity, some cross-sensitivity to H2
Response time: <30 seconds
Operating temperature: -10 to +50°C (sensor element, not chassis)
Heating cycle: 60s at 5V (high heat) + 90s at 1.4V (low heat) → repeating
Output: Analog voltage (0–5V), requires calibration
```

### MQ-135 (Secondary — Air Quality Validation)

```
Principle:  Semiconductor resistance change with CO, CO2, NH3
Primary use: Validates that MQ-7 CO reading is consistent with
             broader air quality degradation
Output: Analog voltage, used as qualitative cross-check
Not used as primary safety sensor (poor CO selectivity alone)
```

## MQ-7 Heating Cycle (Critical for Accuracy)

The MQ-7 requires a specific heating cycle to give accurate readings:

```c
/* MQ-7 heating cycle — must be implemented in firmware */
#define MQ7_HIGH_HEAT_V   5.0f    /* 5V for 60 seconds */
#define MQ7_LOW_HEAT_V    1.4f    /* 1.4V for 90 seconds */
#define MQ7_HIGH_HEAT_MS  60000
#define MQ7_LOW_HEAT_MS   90000

/* Read CO during LOW HEAT phase only — sensor is in sensitive mode */
/* During HIGH HEAT: sensor cleans itself — readings unreliable */
```

## Calibration Procedure

### Step 1 — Baseline in Clean Air (R0 measurement)

```
R0 = reference resistance in 100% clean air, 20°C, 65% RH

Physical setup:
  Take sensor outdoors, away from any CO source
  Wait 24 hours at stable temperature
  Read ADC value → calculate Rs:
    Rs = ((ADC_VREF / ADC_reading) - 1) × R_LOAD
  R0 = Rs / 3.0   (MQ-7 ratio Rs/R0 in clean air ≈ 3.0 from datasheet)

Store R0 in STM32 flash (non-volatile)
```

### Step 2 — CO Concentration Calculation

```
From MQ-7 datasheet characteristic curve (log-log):
  CO_ppm = a × (Rs/R0)^b

Curve-fit constants from datasheet:
  a = 99.042
  b = -1.518

In firmware:
  float Rs = ((VREF / adc_voltage) - 1.0f) * R_LOAD;
  float ratio = Rs / R0_stored;
  float co_ppm = 99.042f * powf(ratio, -1.518f);
```

### Step 3 — Temperature/Humidity Correction

At 175°C environment, MQ-7 internal temperature is elevated.
Correction factor from datasheet Figure 4:

```c
/* Temperature correction for MQ-7 */
float MQ7_TempHumidCorrect(float co_raw, float temp_c, float humid_pct) {
    /* Reference: 20°C, 65% RH → correction factor = 1.0 */
    float temp_factor  = 1.0f + 0.005f * (temp_c - 20.0f);
    float humid_factor = 1.0f + 0.003f * (humid_pct - 65.0f);
    return co_raw / (temp_factor * humid_factor);
}
```

## CO Safety Thresholds (OSHA / WHO Standards)

| Level | PPM | Action | Source |
|---|---|---|---|
| Safe (TLV) | < 25 ppm | Normal operation | ACGIH TLV-TWA |
| Warning | 25–50 ppm | Alert notification | OSHA TWA |
| Alarm | 50–200 ppm | Close valve + buzzer + app alert | OSHA PEL |
| Emergency | > 200 ppm | Immediate shutdown + lockout | NIOSH IDLH = 1200ppm |
| Dangerous | > 1200 ppm | Life-threatening — call emergency | NIOSH IDLH |

System thresholds:
- **WARNING threshold: 35 ppm** (conservative — alerts before OSHA limit)
- **ALARM threshold: 50 ppm** (close valve, buzzer)
- **EMERGENCY threshold: 200 ppm** (lockout, cannot restart remotely)
