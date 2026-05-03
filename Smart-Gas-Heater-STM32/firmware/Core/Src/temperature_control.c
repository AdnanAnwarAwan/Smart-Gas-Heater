/**
 * @file    temperature_control.c
 * @brief   PID temperature control — solenoid valve duty cycle
 * @author  Adnan Anwar Awan
 */

#include "temperature_control.h"
#include "gas_valve.h"
#include "main.h"
#include <math.h>

typedef struct {
    float Kp, Ki, Kd;
    float integral;
    float prev_measured;
    float out_min, out_max;
    float int_clamp;
} PID_t;

static PID_t pid = {
    .Kp = 8.0f, .Ki = 0.1f, .Kd = 2.0f,
    .integral = 0.0f, .prev_measured = 20.0f,
    .out_min = 0.0f, .out_max = 80.0f,  /* Never >80% duty */
    .int_clamp = 50.0f
};

static float setpoint = 20.0f;
#define CLAMP(x, lo, hi) ((x)<(lo)?(lo):((x)>(hi)?(hi):(x)))

void TEMP_ControlInit(void) {
    pid.integral      = 0.0f;
    pid.prev_measured = 20.0f;
}

void TEMP_SetSetpoint(float sp) {
    setpoint = CLAMP(sp, 5.0f, 30.0f);
    pid.integral = 0.0f;   /* Reset integrator on setpoint change */
}

float TEMP_PIDUpdate(float sp, float measured) {
    float dt    = 2.0f;    /* DHT22 2s sample interval */
    float error = sp - measured;

    /* Proportional */
    float p = pid.Kp * error;

    /* Integral with anti-windup */
    pid.integral += error * dt;
    pid.integral   = CLAMP(pid.integral, -pid.int_clamp, pid.int_clamp);
    float i = pid.Ki * pid.integral;

    /* Derivative on measurement (prevents D-kick on setpoint change) */
    float d = pid.Kd * (-(measured - pid.prev_measured) / dt);
    pid.prev_measured = measured;

    float output = CLAMP(p + i + d, pid.out_min, pid.out_max);

    /* Force valve closed if already above setpoint */
    if (measured > sp + 2.0f) {
        output = 0.0f;
        pid.integral = 0.0f;
    }

    return output;
}

/* ─────────────────────────────────────────────────────────────────────── */

/**
 * @file    flame_supervisor.c
 * @brief   IEC 60335-2-102 §6.5 flame supervision state machine
 * @author  Adnan Anwar Awan
 */

#include "flame_supervisor.h"
#include "gas_valve.h"
#include "main.h"
#include <stdio.h>

#define IGNITION_WINDOW_MS   5000U    /* 5s to detect flame */
#define FLAME_STABLE_MS       500U    /* Flame must hold 500ms */
#define FLAME_LOSS_MS         500U    /* 500ms loss before trip */
#define SPARK_PULSES             5
#define SPARK_PERIOD_MS        200U   /* 5 pulses over 1s */

static FlameState_t flame_state = FLAME_IDLE;
static uint32_t     state_start = 0;
static uint32_t     flame_start = 0;
static uint32_t     loss_start  = 0;
static uint8_t      spark_count = 0;
static uint32_t     last_spark  = 0;

extern void FLAME_SuccessCallback(void);
extern void FLAME_FailureCallback(void);

void FLAME_SupervisorInit(void) {
    flame_state = FLAME_IDLE;
}

void FLAME_SupervisorStart(void) {
    flame_state = FLAME_IGNITING;
    state_start = HAL_GetTick();
    spark_count = 0;
    last_spark  = 0;
    VALVE_SetDuty(15.0f);   /* 15% — controlled gas flow during ignition */
    printf("Flame supervisor: IGNITING\r\n");
}

void FLAME_SupervisorStop(void) {
    flame_state = FLAME_IDLE;
    HAL_GPIO_WritePin(GPIOB, IGNITER_PIN, GPIO_PIN_RESET);
    VALVE_Close();
}

/**
 * @brief  Run flame supervision — call every 100ms from main loop
 */
void FLAME_SupervisorUpdate(void) {
    uint32_t now = HAL_GetTick();
    bool flame_detected = (HAL_GPIO_ReadPin(GPIOA, FLAME_SENSOR_PIN) == GPIO_PIN_SET);

    switch (flame_state) {

        case FLAME_IGNITING:
            /* Fire spark pulses */
            if (now - last_spark >= SPARK_PERIOD_MS && spark_count < SPARK_PULSES) {
                HAL_GPIO_WritePin(GPIOB, IGNITER_PIN, GPIO_PIN_SET);
                HAL_Delay(5);   /* 5ms spark pulse */
                HAL_GPIO_WritePin(GPIOB, IGNITER_PIN, GPIO_PIN_RESET);
                spark_count++;
                last_spark = now;
                printf("Spark %d/%d\r\n", spark_count, SPARK_PULSES);
            }

            if (flame_detected) {
                flame_state = FLAME_SUPERVISING;
                flame_start = now;
                printf("Flame detected — supervising\r\n");
            } else if (now - state_start >= IGNITION_WINDOW_MS) {
                printf("Ignition timeout — no flame\r\n");
                FLAME_SupervisorStop();
                flame_state = FLAME_FAILED;
                FLAME_FailureCallback();
            }
            break;

        case FLAME_SUPERVISING:
            if (!flame_detected) {
                printf("Flame lost during supervision\r\n");
                FLAME_SupervisorStop();
                flame_state = FLAME_FAILED;
                FLAME_FailureCallback();
            } else if (now - flame_start >= FLAME_STABLE_MS) {
                flame_state = FLAME_RUNNING;
                FLAME_SuccessCallback();
            }
            break;

        case FLAME_RUNNING:
            if (!flame_detected) {
                if (loss_start == 0) loss_start = now;
                if (now - loss_start >= FLAME_LOSS_MS) {
                    printf("Flame loss confirmed — shutting down\r\n");
                    VALVE_Close();
                    flame_state = FLAME_FAILED;
                    loss_start  = 0;
                    FLAME_FailureCallback();
                }
            } else {
                loss_start = 0;   /* Flame present — reset loss timer */
            }
            break;

        default: break;
    }
}

/* ─────────────────────────────────────────────────────────────────────── */

/**
 * @file    gas_valve.c
 * @brief   24V solenoid valve — normally closed, PWM modulated
 * @author  Adnan Anwar Awan
 */

#include "gas_valve.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;
#define TIM3_PERIOD  999U    /* 20Hz PWM @ 20kHz TIM3 clock */
#define CLAMP(x,a,b) ((x)<(a)?(a):((x)>(b)?(b):(x)))

void VALVE_Init(void) {
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    VALVE_Close();   /* Start with valve CLOSED — fail-safe */
}

void VALVE_SetDuty(float duty_pct) {
    duty_pct = CLAMP(duty_pct, 0.0f, 80.0f);  /* Hard max 80% */
    uint32_t ccr = (uint32_t)(duty_pct / 100.0f * TIM3_PERIOD);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
}

void VALVE_Close(void) {
    VALVE_SetDuty(0.0f);
}

void VALVE_OpenFull(void) {
    VALVE_SetDuty(80.0f);   /* Never 100% — safety margin */
}

/* ─────────────────────────────────────────────────────────────────────── */

/**
 * @file    safety_monitor.c
 * @brief   Safety flag management and PIN validation for lockout reset
 * @author  Adnan Anwar Awan
 */

#include "safety_monitor.h"
#include "main.h"
#include <string.h>

#define SURFACE_ALARM_TEMP   175.0f
#define SURFACE_TRIP_TEMP    185.0f
#define CO_WARNING_PPM        35.0f
#define CO_ALARM_PPM          50.0f
#define CO_EMERGENCY_PPM     200.0f

/* PIN stored in flash — default "1234" */
static const char LOCKOUT_PIN[] = "1234";

void SAFETY_Init(void) {
    /* Nothing to init — flags set externally */
}

void SAFETY_Update(SafetyFlags_t *flags, float co_ppm, float t_surface) {
    *flags = SAFETY_OK;

    if (co_ppm > CO_EMERGENCY_PPM) *flags |= SAFETY_CO_EMERGENCY;
    else if (co_ppm > CO_ALARM_PPM) *flags |= SAFETY_CO_ALARM;
    else if (co_ppm > CO_WARNING_PPM) *flags |= SAFETY_CO_WARNING;

    if (t_surface > SURFACE_TRIP_TEMP)  *flags |= SAFETY_OVERTEMP;
    else if (t_surface > SURFACE_ALARM_TEMP) *flags |= SAFETY_OVERTEMP_WARN;
}

bool SAFETY_ValidatePIN(const char *pin) {
    return (strncmp(pin, LOCKOUT_PIN, 4) == 0);
}

/* ─────────────────────────────────────────────────────────────────────── */

/**
 * @file    wifi_telemetry.c
 * @brief   ESP32-C3 interface — telemetry, alerts, command reception
 * @author  Adnan Anwar Awan
 */

#include "wifi_telemetry.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart1;
static char rx_buf[128];
static bool cmd_ready = false;

void WIFI_Init(void) {
    HAL_UART_Transmit(&huart1, (uint8_t*)"SYNC\r\n", 6, 200);
    HAL_Delay(500);
    printf("ESP32 handshake sent\r\n");
}

void WIFI_SendTelemetry(float t_amb, float t_surf, float co,
                        float humid, HeaterState_t state, SafetyFlags_t flags) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf),
        "TEL:{\"t_amb\":%.1f,\"t_surf\":%.1f,\"co\":%.1f,"
        "\"humid\":%.1f,\"state\":%d,\"flags\":%d}\r\n",
        t_amb, t_surf, co, humid, (int)state, (int)flags);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, 200);
}

void WIFI_SendAlert(const char *type, float value) {
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "ALERT:{\"type\":\"%s\",\"value\":%.1f}\r\n", type, value);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, 200);
}

void WIFI_SendReply(const char *reply) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "REPLY:%s\r\n", reply);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, 100);
}

void WIFI_LogDataPoint(float t_amb, float t_surf, float co, float setpoint) {
    char buf[160];
    int len = snprintf(buf, sizeof(buf),
        "LOG:{\"t_amb\":%.1f,\"t_surf\":%.1f,\"co\":%.1f,\"sp\":%.1f}\r\n",
        t_amb, t_surf, co, setpoint);
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, len, 200);
}

bool WIFI_CommandReady(char *out_buf, uint32_t buf_len) {
    /* Simplified — real implementation uses UART IDLE interrupt */
    if (!cmd_ready) return false;
    strncpy(out_buf, rx_buf, buf_len - 1);
    cmd_ready = false;
    return true;
}
