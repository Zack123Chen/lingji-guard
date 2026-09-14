#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"

extern char GPS_Buffer[128];
extern uint8_t GPS_Flag;
extern float current_lat;
extern float current_lng;

void Parse_GPS_Data(char* str);
void USART3_Init(uint32_t bound); // GPS Ä£¿é
void USART3_SendString(char *str);
#endif
