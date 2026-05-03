#pragma once
#include "main.h"
void SAFETY_Init(void);
void SAFETY_Update(SafetyFlags_t *flags, float co_ppm, float t_surface);
bool SAFETY_ValidatePIN(const char *pin);
