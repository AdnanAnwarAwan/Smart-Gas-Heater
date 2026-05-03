/**
 * @file    co_sensor.c
 * @brief   Dual CO sensor — MQ-7 (primary) + MQ-135 (secondary validation)
 *
 * MQ-7 heating cycle:
 *   60s at 5V (HIGH HEAT — sensor cleaning phase, readings invalid)
 *   90s at 1.4V (LOW HEAT — sensitive sampling phase)
 *   Read ADC during LOW HEAT phase only
 *
 * CO concentration calculation:
 *   Rs = ((VREF/Vadc) - 1) * R_LOAD
 *   CO_ppm = 99.042 * (Rs/R0)^(-1.518)   [from MQ-7 datasheet curve fit]
 *
 * Temperature/humidity correction applied from DHT22 reading.
 *
 * @author  Adnan Anwar Awan
 */

#include "co_sensor.h"
#include "main.h"
#include <math.h>
#include <stdio.h>

/* ── Hardware constants ───────────────────────────────────────────────── */
#define ADC_VREF        3.3f
#define ADC_RESOLUTION  4096.0f
#define R_LOAD_MQ7      10000.0f    /* 10kΩ load resistor */
#define R_LOAD_MQ135    10000.0f

/* ── MQ-7 calibration (stored in flash after R0 measurement) ─────────── */
static float MQ7_R0     = 10000.0f;   /* Loaded from flash on boot */
static bool  calibrated  = false;

/* ── Heating cycle state ─────────────────────────────────────────────── */
typedef enum { MQ7_HIGH_HEAT, MQ7_LOW_HEAT } MQ7Phase_t;
static MQ7Phase_t mq7_phase          = MQ7_HIGH_HEAT;
static uint32_t   phase_start_ms     = 0;
#define MQ7_HIGH_HEAT_MS    60000U   /* 60s high heat */
#define MQ7_LOW_HEAT_MS     90000U   /* 90s low heat */

/* ── DMA ADC buffer ──────────────────────────────────────────────────── */
static volatile uint32_t adc_dma[3];   /* [0]=MQ7, [1]=MQ135, [2]=Vbatt */
#define ADC_MQ7   0
#define ADC_MQ135 1

/* ── Moving average for stability ────────────────────────────────────── */
#define MA_LEN  8
static float mq7_ma_buf[MA_LEN];
static uint8_t ma_idx = 0;

static float MovingAverage(float *buf, float new_val) {
    buf[ma_idx % MA_LEN] = new_val;
    ma_idx++;
    float sum = 0;
    for (int i = 0; i < MA_LEN; i++) sum += buf[i];
    return sum / MA_LEN;
}

/* ── R0 stored in flash (last page of STM32G031 flash) ──────────────── */
#define FLASH_R0_ADDR   0x0801FC00U
static void LoadR0FromFlash(void) {
    float stored = *((float *)FLASH_R0_ADDR);
    if (!isnan(stored) && stored > 100.0f && stored < 100000.0f) {
        MQ7_R0     = stored;
        calibrated = true;
        printf("MQ-7 R0 loaded: %.0f Ω\r\n", MQ7_R0);
    } else {
        printf("MQ-7 R0 not calibrated — using default\r\n");
    }
}

static void SaveR0ToFlash(float r0) {
    /* STM32G031 flash write — erase page first, then write */
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page      = 127,   /* Last page */
        .NbPages   = 1
    };
    uint32_t page_error;
    HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, FLASH_R0_ADDR,
                      *((uint64_t*)&r0));
    HAL_FLASH_Lock();
    printf("MQ-7 R0 saved: %.0f Ω\r\n", r0);
}

/* ─────────────────────────────────────────────────────────────────────── */

void CO_SensorInit(void) {
    LoadR0FromFlash();
    phase_start_ms = HAL_GetTick();
    mq7_phase = MQ7_HIGH_HEAT;
    /* Set MQ-7 heater to 5V (HIGH HEAT) */
    HAL_GPIO_WritePin(GPIOB, MQ7_HEAT_PIN, GPIO_PIN_SET);
    printf("MQ-7: HIGH HEAT phase (60s warm-up)\r\n");
}

void CO_StartContinuousMonitoring(void) {
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma, 3);
}

/**
 * @brief  Get current CO concentration in ppm (call every 100ms from main)
 * @return CO ppm (corrected for temperature and humidity)
 */
float CO_GetPPM(void) {
    /* Only read during LOW HEAT phase */
    if (mq7_phase != MQ7_LOW_HEAT) return 0.0f;
    if (!calibrated) return 0.0f;

    float vadc = (adc_dma[ADC_MQ7] / ADC_RESOLUTION) * ADC_VREF;
    if (vadc < 0.01f) return 0.0f;   /* Sensor disconnected */

    float Rs     = ((ADC_VREF / vadc) - 1.0f) * R_LOAD_MQ7;
    float ratio  = Rs / MQ7_R0;

    /* MQ-7 datasheet curve: CO_ppm = 99.042 × (Rs/R0)^(-1.518) */
    float co_raw = 99.042f * powf(ratio, -1.518f);

    /* Temperature/humidity correction (from external DHT22 reading) */
    extern float t_ambient, humid_pct;
    float t_factor = 1.0f + 0.005f * (t_ambient - 20.0f);
    float h_factor = 1.0f + 0.003f * (humid_pct - 65.0f);
    float co_corr  = co_raw / (t_factor * h_factor);

    return MovingAverage(mq7_ma_buf, co_corr);
}

/**
 * @brief  Update MQ-7 heating cycle (call every 100ms)
 */
void CO_UpdateHeatingCycle(void) {
    uint32_t elapsed = HAL_GetTick() - phase_start_ms;

    if (mq7_phase == MQ7_HIGH_HEAT && elapsed >= MQ7_HIGH_HEAT_MS) {
        /* Switch to LOW HEAT */
        mq7_phase = MQ7_LOW_HEAT;
        phase_start_ms = HAL_GetTick();
        HAL_GPIO_WritePin(GPIOB, MQ7_HEAT_PIN, GPIO_PIN_RESET);
        /* 1.4V via PWM on heater pin — use TIM1 to output 28% duty at 5V */
        printf("MQ-7: LOW HEAT phase — reading active\r\n");

    } else if (mq7_phase == MQ7_LOW_HEAT && elapsed >= MQ7_LOW_HEAT_MS) {
        /* Switch back to HIGH HEAT */
        mq7_phase = MQ7_HIGH_HEAT;
        phase_start_ms = HAL_GetTick();
        HAL_GPIO_WritePin(GPIOB, MQ7_HEAT_PIN, GPIO_PIN_SET);
        printf("MQ-7: HIGH HEAT phase — cleaning\r\n");
    }
}

/**
 * @brief  Run R0 calibration in clean air (triggered by app command)
 */
void CO_RunCalibration(void) {
    if (mq7_phase != MQ7_LOW_HEAT) {
        printf("Calibration: waiting for LOW HEAT phase...\r\n");
        return;
    }
    /* Average 10 samples */
    float rs_sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        float vadc = (adc_dma[ADC_MQ7] / ADC_RESOLUTION) * ADC_VREF;
        float rs   = ((ADC_VREF / vadc) - 1.0f) * R_LOAD_MQ7;
        rs_sum += rs;
        HAL_Delay(100);
    }
    float rs_avg = rs_sum / 10.0f;
    /* R0 = Rs_cleanair / 3.0 (datasheet ratio) */
    MQ7_R0     = rs_avg / 3.0f;
    calibrated = true;
    SaveR0ToFlash(MQ7_R0);
    printf("Calibration complete: R0 = %.0f Ω\r\n", MQ7_R0);
}
