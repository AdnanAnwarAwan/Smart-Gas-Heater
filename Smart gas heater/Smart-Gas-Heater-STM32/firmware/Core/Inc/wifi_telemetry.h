#pragma once
#include "main.h"
#include <stdbool.h>
void WIFI_Init(void);
void WIFI_SendTelemetry(float t_amb, float t_surf, float co,
                        float humid, HeaterState_t state, SafetyFlags_t flags);
void WIFI_SendAlert(const char *type, float value);
void WIFI_SendReply(const char *reply);
void WIFI_LogDataPoint(float t_amb, float t_surf, float co, float setpoint);
bool WIFI_CommandReady(char *out_buf, uint32_t buf_len);
