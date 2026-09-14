#include "GPS.h"
#include "main.h"

float current_lat = 45.750000;
float current_lng = 126.630000;

void Parse_GPS_Data(char* str) {
    char *p = strstr(str, "$GNRMC");
    if(!p) p = strstr(str, "$GPRMC");
    if(!p) return; 

    if(strstr(p, ",A,")) { 
        int comma = 0, lat_idx = 0, lng_idx = 0;
        char lat_s[15] = {0}, lng_s[15] = {0};

        for(int i = 0; p[i] != '\0' && p[i] != '\n'; i++) {
            if(p[i] == ',') { comma++; continue; }
            if(comma == 3 && lat_idx < 14) lat_s[lat_idx++] = p[i];
            if(comma == 5 && lng_idx < 14) lng_s[lng_idx++] = p[i];
        }

        if(lat_idx > 0 && lng_idx > 0) {
            float lat_raw = atof(lat_s);
            float lng_raw = atof(lng_s);
            current_lat = (int)(lat_raw / 100) + (lat_raw - (int)(lat_raw / 100) * 100) / 60.0;
            current_lng = (int)(lng_raw / 100) + (lng_raw - (int)(lng_raw / 100) * 100) / 60.0;
        }
    }
}

// 串口 3 初始化 (PB10=TX, PB11=RX)
void USART3_Init(uint32_t bound) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); 
    USART_Cmd(USART3, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}


// GPS 数据缓冲区
char GPS_Buffer[128];
uint8_t GPS_Idx = 0;
uint8_t GPS_Flag = 0;

void USART3_IRQHandler(void) {
    uint8_t res;
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
        res = USART_ReceiveData(USART3);
        if(res == '$') { GPS_Idx = 0; } // 帧头
        GPS_Buffer[GPS_Idx++] = res;
        if(res == '\n') { GPS_Flag = 1; } // 帧尾
    }
}
