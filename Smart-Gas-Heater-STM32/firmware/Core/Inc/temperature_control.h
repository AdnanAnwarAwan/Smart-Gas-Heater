#pragma once
void  TEMP_ControlInit(void);
void  TEMP_SetSetpoint(float sp);
float TEMP_PIDUpdate(float setpoint, float measured);
