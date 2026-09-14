#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

#define OLED_SCL_PORT    GPIOB
#define OLED_SCL_PIN     GPIO_Pin_6
#define OLED_SDA_PORT    GPIOB
#define OLED_SDA_PIN     GPIO_Pin_7

#define OLED_W_SCL(x)    GPIO_WriteBit(OLED_SCL_PORT, OLED_SCL_PIN, (BitAction)(x))
#define OLED_W_SDA(x)    GPIO_WriteBit(OLED_SDA_PORT, OLED_SDA_PIN, (BitAction)(x))

extern const uint8_t OLED_F8x16[][16];

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_Force_Flip(void);

#endif
