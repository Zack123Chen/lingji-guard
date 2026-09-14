#ifndef __BUTTON_H
#define __BUTTON_H

#include "stm32f10x.h"

// --- 动作枚举 ---
typedef enum {
    BTN_NONE = 0,
    BTN_0_CLICK, // WKUP (PA0)
    BTN_1_CLICK, // UP (PA6)
    BTN_2_CLICK, // DOWN (PA7)
    BTN_3_CLICK, // LEFT (PB8)
    BTN_4_CLICK  // RIGHT (PB9)
} BtnAction_t;

// --- 函数声明 (完全独立) ---
void Button0_Init(void);
BtnAction_t Button0_Scan(void);

void Button1_Init(void);
BtnAction_t Button1_Scan(void);

void Button2_Init(void);
BtnAction_t Button2_Scan(void);

void Button3_Init(void);
BtnAction_t Button3_Scan(void);

void Button4_Init(void);
BtnAction_t Button4_Scan(void);

void vKeyTask(void *pvParameters);

#endif
