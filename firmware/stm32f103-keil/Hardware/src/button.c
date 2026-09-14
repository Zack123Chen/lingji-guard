#include "button.h"
#include "main.h"

// ================= B0 (PA0) WKUP =================
void Button0_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
BtnAction_t Button0_Scan(void) {
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == SET) {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == SET) {
            while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == SET);
            return BTN_0_CLICK;
        }
    }
    return BTN_NONE;
}

// ================= B1 (PA6) UP =================
void Button1_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
BtnAction_t Button1_Scan(void) {
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == SET) {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == SET) {
            while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_6) == SET);
            return BTN_1_CLICK;
        }
    }
    return BTN_NONE;
}

// ================= B2 (PA7) DOWN =================
void Button2_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}
BtnAction_t Button2_Scan(void) {
    if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == SET) {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == SET) {
            while (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_7) == SET);
            return BTN_2_CLICK;
        }
    }
    return BTN_NONE;
}

// ================= B3 (PB8) LEFT =================
void Button3_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}
BtnAction_t Button3_Scan(void) {
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) == SET) {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) == SET) {
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8) == SET);
            return BTN_3_CLICK;
        }
    }
    return BTN_NONE;
}

// ================= B4 (PB9) RIGHT =================
void Button4_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}
BtnAction_t Button4_Scan(void) {
    if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == SET) {
        Delay_ms(20);
        if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == SET) {
            while (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_9) == SET);
            return BTN_4_CLICK;
        }
    }
    return BTN_NONE;
}

void vKeyTask(void *pvParameters) {
    Button0_Init();
    Button1_Init();
    Button2_Init();
    Button3_Init();
    Button4_Init();

    for(;;) {
        // 使用 else if 确保一次只处理一个按键，防止逻辑打架
        if (Button0_Scan() == BTN_0_CLICK) {
            g_ui_page = 0;
            g_page_changed = 1;
        }
        else if (Button2_Scan() == BTN_2_CLICK) {
            g_ui_page = (g_ui_page + 1) % MAX_PAGES;
            g_page_changed = 1;
        }
        else if (Button1_Scan() == BTN_1_CLICK) {
            if(g_ui_page > 0) g_ui_page--;
            else g_ui_page = MAX_PAGES - 1;
            g_page_changed = 1;
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // 适当加长间隔，提高系统整体效率
    }
}
