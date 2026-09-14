#include "ch9141k.h"
#include "main.h"

void CH9141K_Init(uint32_t baudrate) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    // 1. 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);

    // 2. 配置控制引脚: PB5 (AT), PA8 (SLEEP)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    
    GPIO_InitStructure.GPIO_Pin = CH_AT_PIN;
    GPIO_Init(CH_AT_PORT, &GPIO_InitStructure);
    
    GPIO_InitStructure.GPIO_Pin = CH_SLEEP_PIN;
    GPIO_Init(CH_SLEEP_PORT, &GPIO_InitStructure);

    // 3. 配置USART1引脚: PA9 (TX) 为推挽复用, PA10 (RX) 为浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 配置USART1参数
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);
		
		NVIC_InitTypeDef NVIC_InitStructure;
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // 开启接收中断

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

    // 5. 使能串口
    USART_Cmd(USART1, ENABLE);

    // 默认状态：透传模式，唤醒状态
    CH9141_AT_LOW();
    CH9141_WAKEUP();
}

void CH9141K_SendByte(uint8_t Byte) {
    // 关键：等待发送寄存器空
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
    USART_SendData(USART1, Byte);
}

void CH9141K_SendString(char *str) {
    while (*str) {
        CH9141K_SendByte((uint8_t)*str++);
    }
    // 关键：等待这一串数据完全物理发出
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
}

void USART1_IRQHandler(void) {
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t res = USART_ReceiveData(USART1);
        // 在这里处理收到的字节（例如存入缓冲区）
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
