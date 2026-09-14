#ifndef __CH9141K_H
#define __CH9141K_H

#include "stm32f10x.h"

// 引脚宏定义
#define CH_AT_PORT        GPIOB
#define CH_AT_PIN         GPIO_Pin_5

#define CH_SLEEP_PORT     GPIOA
#define CH_SLEEP_PIN      GPIO_Pin_8

// 模式控制
#define CH9141_AT_HIGH()  GPIO_SetBits(CH_AT_PORT, CH_AT_PIN)      // AT模式
#define CH9141_AT_LOW()   GPIO_ResetBits(CH_AT_PORT, CH_AT_PIN)    // 透传模式

#define CH9141_WAKEUP()   GPIO_ResetBits(CH_SLEEP_PORT, CH_SLEEP_PIN) // 唤醒
#define CH9141_SLEEP()    GPIO_SetBits(CH_SLEEP_PORT, CH_SLEEP_PIN)   // 睡眠

// 函数声明
void CH9141K_Init(uint32_t baudrate);
void CH9141K_SendByte(uint8_t data);
void CH9141K_SendString(char *str);
void USART1_IRQHandler(void);

#endif
