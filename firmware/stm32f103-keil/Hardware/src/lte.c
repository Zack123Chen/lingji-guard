#include "LTE.h"
#include "main.h"

void Module_4G_Init(void) {
    USART2_SendString("AT+QMTDISC=0\r\n"); Delay_ms(500);
    USART2_SendString("AT+QMTCFG=\"pdpcid\",0,1\r\n"); Delay_ms(500);
    USART2_SendString("AT+QMTOPEN=0,\"broker-cn.emqx.io\",1883\r\n"); Delay_ms(3000); 
    USART2_SendString("AT+QMTCONN=0,\"HIT_Collar_Final_777\"\r\n"); Delay_ms(2000);
}

// 串口 2 初始化 (PA2=TX, PA3=RX) —— 这是新补上的！
void USART2_Init(uint32_t bound) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 1. 开启时钟 (注意：USART2 在 APB1 总线上)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. PA2 (TX2) 配置为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. PA3 (RX2) 配置为浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 串口 2 参数配置
    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_Cmd(USART2, ENABLE);
}

void USART2_SendByte(uint8_t ch) {
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, ch);
}

void USART2_SendString(char *str) {
    while (*str) {
        USART2_SendByte(*str++);
    }
}
