#ifndef __LTE_H
#define __LTE_H

#include "stm32f10x.h"

void Module_4G_Init(void);
void USART2_Init(uint32_t bound);
void USART2_SendByte(uint8_t ch);
void USART2_SendString(char *str);
	
#endif
