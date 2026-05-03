/**
 * @file    main.c
 * @brief   Smart Gas Heater — STM32G031 Industrial Firmware
 *
 * Safety design principles:
 *   1. Solenoid valve is NORMALLY CLOSED — fail-safe on power loss
 *   2. IWDG hardware watchdog — MCU reset if firmware hangs (valve closes)
 *   3. CO check runs every 20ms in ADC ISR — independent of main loop
 *   4. Flame supervisor uses hardware timer — not software delay
 *   5. Hardware OT interlock (LM393 comparator) fires independently of MCU
 *
 * IEC 60335-2-102 compliance:
 *   - Flame supervision §6.5.2 (5s ignition window, 3 retries, lockout)
 *   - Automatic gas shutoff on CO detection
 *   - Manual reset required after lockout
 *
 * @author  Adnan Anwar Awan
 */

#include "main.h"
#include "co_sensor.h"
#include "temperature_control.h"
#include "flame_supervisor.h"
#include "gas_valve.h"
#include "safety_monitor.h"
#include "wifi_telemetry.h"
#include <stdio.h>
#include <string.h>

/* ── HAL handles (CubeMX generated) ──────────────────────────────────── */
ADC_HandleTypeDef  hadc1;
DMA_HandleTypeDef  hdma_adc1;
TIM_HandleTypeDef  htim1;     /* PID tick 100ms */
TIM_HandleTypeDef  htim2;     /* Flame supervision 5s timeout */
TIM_HandleTypeDef  htim3;     /* Solenoid valve PWM */
UART_HandleTypeDef huart1;    /* ESP32-C3 telemetry */
UART_HandleTypeDef huart2;    /* Debug */
SPI_HandleTypeDef  hspi1;     /* MAX31855 thermocouple */
IWDG_HandleTypeDef hiwdg;     /* Hardware watchdog — CRITICAL for safety */

/* ── System state ─────────────────────────────────────────────────────── */
HeaterState_t     heater_state   = STATE_IDLE;
SafetyFlags_t     safety_flags   = SAFETY_OK;
float             t_setpoint     = 20.0f;
float             t_ambient      = 20.0f;
float             t_surface      = 25.0f;
float             co_ppm         = 0.0f;
float             humid_pct      = 50.0f;
uint8_t           ignite_attempts = 0;

/* ── Timestamps ───────────────────────────────────────────────────────── */
static uint32_t last_dht_ms   = 0;
static uint32_t last_telem_ms = 0;
static uint32_t last_log_ms   = 0;

/* ─────────────────────────────────────────────────────────────────────────
 * MAIN
 * ───────────────────────────────────────────────────────────────────────── */
int main(void) {
    HAL_Init();
    SystemClock_Config();   /* 64MHz (STM32G031 max) */

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_ADC1_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_SPI1_Init();
    MX_IWDG_Init();         /* 2s hardware watchdog — NEVER disable */

    /* Subsystem init */
    CO_SensorInit();
    TEMP_ControlInit();
    FLAME_SupervisorInit();
    VALVE_Init();           /* Solenoid starts CLOSED */
    SAFETY_Init();
    WIFI_Init();

    /* Status LED — green = system healthy */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);

    printf("\r\n=== Smart Gas Heater STM32G031 ===\r\n");
    printf("Safety: IEC 60335-2-102 | CO thresholds: 35/50/200ppm\r\n");
    printf("PCB: Polyimide | Solder: AuSn | MCU: SOI process\r\n");
    printf("State: IDLE — awaiting ignition command\r\n\r\n");

    /* Start CO monitoring ADC (runs continuously in background via DMA) */
    CO_StartContinuousMonitoring();

    /* ── Main loop ─────────────────────────────────────────────────────── */
    while (1) {

        /* ① Pet the watchdog every loop — if not pet, MCU resets → valve closes */
        HAL_IWDG_Refresh(&hiwdg);

        uint32_t now = HAL_GetTick();

        /* ② Check safety conditions (CO, surface temperature) */
        SAFETY_Update(&safety_flags, co_ppm, t_surface);

        /* ③ Handle safety trip */
        if (safety_flags != SAFETY_OK && heater_state == STATE_RUNNING) {
            VALVE_Close();
            FLAME_SupervisorStop();

            if (safety_flags & SAFETY_CO_ALARM) {
                heater_state = STATE_IDLE;
                HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, GPIO_PIN_SET); /* Buzzer */
                WIFI_SendAlert("CO_ALARM", co_ppm);
                printf("CO ALARM: %.1f ppm — valve closed\r\n", co_ppm);
            }
            if (safety_flags & SAFETY_CO_EMERGENCY) {
                heater_state = STATE_LOCKOUT;
                WIFI_SendAlert("CO_EMERGENCY_LOCKOUT", co_ppm);
                printf("CO EMERGENCY: %.1f ppm — LOCKOUT\r\n", co_ppm);
            }
            if (safety_flags & SAFETY_OVERTEMP) {
                heater_state = STATE_IDLE;
                WIFI_SendAlert("OVERTEMP", t_surface);
                printf("SURFACE OVERTEMP: %.1f°C\r\n", t_surface);
            }
        }

        /* ④ Run PID temperature control when heater is running */
        if (heater_state == STATE_RUNNING && safety_flags == SAFETY_OK) {
            float valve_duty = TEMP_PIDUpdate(t_setpoint, t_ambient);
            VALVE_SetDuty(valve_duty);
        }

        /* ⑤ DHT22 ambient temperature read every 2s */
        if (now - last_dht_ms >= 2000) {
            last_dht_ms = now;
            DHT22_Read(&t_ambient, &humid_pct);
        }

        /* ⑥ Surface temperature via MAX31855 every 500ms */
        static uint32_t last_surface = 0;
        if (now - last_surface >= 500) {
            last_surface = now;
            t_surface = MAX31855_ReadTemp();
        }

        /* ⑦ Process commands from ESP32 (Wi-Fi / app) */
        ProcessWifiCommand();

        /* ⑧ Telemetry to Firebase every 500ms */
        if (now - last_telem_ms >= 500) {
            last_telem_ms = now;
            WIFI_SendTelemetry(t_ambient, t_surface, co_ppm,
                               humid_pct, heater_state, safety_flags);
        }

        /* ⑨ Data log every 60s */
        if (now - last_log_ms >= 60000) {
            last_log_ms = now;
            WIFI_LogDataPoint(t_ambient, t_surface, co_ppm, t_setpoint);
        }
    }
}

/* ── Process command from ESP32 ─────────────────────────────────────── */
void ProcessWifiCommand(void) {
    static char cmd_buf[64];
    static uint8_t cmd_byte;

    /* Non-blocking: check if a complete command line arrived */
    if (!WIFI_CommandReady(cmd_buf, sizeof(cmd_buf))) return;

    printf("CMD: %s\r\n", cmd_buf);

    if (strncmp(cmd_buf, "IGNITE", 6) == 0) {
        if (heater_state == STATE_LOCKOUT) {
            printf("LOCKOUT — cannot ignite remotely\r\n");
            WIFI_SendReply("REJECTED:LOCKOUT");
            return;
        }
        if (safety_flags != SAFETY_OK) {
            printf("Safety fault active — cannot ignite\r\n");
            WIFI_SendReply("REJECTED:SAFETY");
            return;
        }
        ignite_attempts = 0;
        heater_state    = STATE_IGNITING;
        FLAME_SupervisorStart();

    } else if (strncmp(cmd_buf, "SHUTDOWN", 8) == 0) {
        VALVE_Close();
        FLAME_SupervisorStop();
        heater_state = STATE_IDLE;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET); /* Heating LED off */
        WIFI_SendReply("OK:SHUTDOWN");

    } else if (strncmp(cmd_buf, "SETTEMP:", 8) == 0) {
        float new_setpoint = atof(&cmd_buf[8]);
        if (new_setpoint >= 5.0f && new_setpoint <= 30.0f) {
            t_setpoint = new_setpoint;
            TEMP_SetSetpoint(t_setpoint);
            printf("Setpoint → %.1f°C\r\n", t_setpoint);
            WIFI_SendReply("OK:SETTEMP");
        } else {
            WIFI_SendReply("REJECTED:RANGE");
        }

    } else if (strncmp(cmd_buf, "RESET_LOCKOUT:", 14) == 0) {
        /* Requires PIN code — security check */
        char *pin = &cmd_buf[14];
        if (SAFETY_ValidatePIN(pin)) {
            heater_state  = STATE_IDLE;
            safety_flags  = SAFETY_OK;
            ignite_attempts = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); /* Alarm LED off */
            WIFI_SendReply("OK:LOCKOUT_CLEARED");
            printf("Lockout cleared by PIN\r\n");
        } else {
            WIFI_SendReply("REJECTED:WRONG_PIN");
        }

    } else if (strncmp(cmd_buf, "CALIBRATE", 9) == 0) {
        CO_RunCalibration();
        WIFI_SendReply("OK:CALIBRATING");
    }
}

/* ── Flame supervisor callback (from flame_supervisor.c) ─────────────── */
void FLAME_SuccessCallback(void) {
    heater_state    = STATE_RUNNING;
    ignite_attempts = 0;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); /* Heating LED on */
    WIFI_SendReply("OK:RUNNING");
    printf("Ignition successful — RUNNING\r\n");
}

void FLAME_FailureCallback(void) {
    VALVE_Close();
    ignite_attempts++;
    printf("Ignition failed (attempt %d/3)\r\n", ignite_attempts);

    if (ignite_attempts >= 3) {
        heater_state = STATE_LOCKOUT;
        safety_flags |= SAFETY_LOCKOUT;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET); /* Alarm LED */
        WIFI_SendAlert("IGNITION_LOCKOUT", (float)ignite_attempts);
        printf("LOCKOUT — 3 failed ignition attempts\r\n");
    } else {
        /* Retry after 30 seconds */
        heater_state = STATE_RETRY_WAIT;
    }
}

/* ── Stub init functions (replace with CubeMX output) ────────────────── */
void SystemClock_Config(void)  { /* 64MHz PLL */ }
void MX_GPIO_Init(void)        { /* All pins per pin map */ }
void MX_DMA_Init(void)         { /* DMA for ADC1 */ }
void MX_ADC1_Init(void)        { /* PA0 MQ-7, PA1 MQ-135, PA2 Vbatt */ }
void MX_TIM1_Init(void)        { /* 100ms PID tick */ }
void MX_TIM2_Init(void)        { /* 5s flame supervision timeout */ }
void MX_TIM3_Init(void)        { /* PWM for solenoid valve (20Hz) */ }
void MX_USART1_UART_Init(void) { /* 115200 ESP32 */ }
void MX_USART2_UART_Init(void) { /* 115200 debug */ }
void MX_SPI1_Init(void)        { /* SPI for MAX31855 */ }
void MX_IWDG_Init(void)        { /* 2000ms watchdog */ }
