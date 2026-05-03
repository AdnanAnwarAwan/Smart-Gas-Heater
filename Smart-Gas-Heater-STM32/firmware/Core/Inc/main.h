#pragma once
#include "stm32g0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATE_IDLE, STATE_IGNITING, STATE_SUPERVISING,
    STATE_RUNNING, STATE_FAILED, STATE_RETRY_WAIT, STATE_LOCKOUT
} HeaterState_t;

typedef enum {
    FLAME_IDLE, FLAME_IGNITING, FLAME_SUPERVISING, FLAME_RUNNING, FLAME_FAILED
} FlameState_t;

typedef uint8_t SafetyFlags_t;
#define SAFETY_OK           0x00
#define SAFETY_CO_WARNING   0x01
#define SAFETY_CO_ALARM     0x02
#define SAFETY_CO_EMERGENCY 0x04
#define SAFETY_OVERTEMP_WARN 0x08
#define SAFETY_OVERTEMP     0x10
#define SAFETY_LOCKOUT      0x20

/* GPIO aliases */
#define FLAME_SENSOR_PIN    GPIO_PIN_3
#define IGNITER_PIN         GPIO_PIN_1
#define MQ7_HEAT_PIN        GPIO_PIN_6

extern ADC_HandleTypeDef  hadc1;
extern TIM_HandleTypeDef  htim1, htim2, htim3;
extern UART_HandleTypeDef huart1, huart2;
extern SPI_HandleTypeDef  hspi1;
extern IWDG_HandleTypeDef hiwdg;
extern HeaterState_t heater_state;
extern SafetyFlags_t safety_flags;
extern float t_setpoint, t_ambient, t_surface, co_ppm, humid_pct;

void ProcessWifiCommand(void);
void FLAME_SuccessCallback(void);
void FLAME_FailureCallback(void);
float DHT22_Read(float *temp, float *humid);
float MAX31855_ReadTemp(void);
void SystemClock_Config(void);
void MX_GPIO_Init(void); void MX_DMA_Init(void); void MX_ADC1_Init(void);
void MX_TIM1_Init(void); void MX_TIM2_Init(void); void MX_TIM3_Init(void);
void MX_USART1_UART_Init(void); void MX_USART2_UART_Init(void);
void MX_SPI1_Init(void); void MX_IWDG_Init(void);
