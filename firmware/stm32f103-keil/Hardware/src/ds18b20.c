#include "ds18b20.h"
#include "Delay.h"

#define DS_PORT GPIOA
#define DS_PIN  GPIO_Pin_1

void DS18B20_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = DS_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS_PORT, &GPIO_InitStructure);
    
    GPIO_SetBits(DS_PORT, DS_PIN); 

    Delay_us(1000); 
    DS18B20_GetTemp();
}

// 产生复位脉冲并读取应答
static uint8_t DS18B20_Reset(void) {
    uint8_t ack;
    GPIO_ResetBits(DS_PORT, DS_PIN); // 拉低总线
    Delay_us(500);                   // 保持500us
    GPIO_SetBits(DS_PORT, DS_PIN);   // 释放总线
    Delay_us(60);                    // 等待传感器拉低响应
    ack = GPIO_ReadInputDataBit(DS_PORT, DS_PIN); // 读应答
    Delay_us(400);                   // 等待时序结束
    return ack;
}

static void DS18B20_WriteByte(uint8_t data) {
    uint8_t i;
    for(i=0; i<8; i++) {
        GPIO_ResetBits(DS_PORT, DS_PIN);
        Delay_us(10);
        if(data & 0x01) GPIO_SetBits(DS_PORT, DS_PIN);
        Delay_us(50);
        GPIO_SetBits(DS_PORT, DS_PIN);
        Delay_us(2);
        data >>= 1;
    }
}

static uint8_t DS18B20_ReadByte(void) {
    uint8_t i, data = 0;
    for(i=0; i<8; i++) {
        GPIO_ResetBits(DS_PORT, DS_PIN);
        Delay_us(2);
        GPIO_SetBits(DS_PORT, DS_PIN);
        Delay_us(10);
        if(GPIO_ReadInputDataBit(DS_PORT, DS_PIN)) data |= (0x01 << i);
        Delay_us(50);
    }
    return data;
}

// 读取上次测好的温度，并马上命令它开始下一次测量（极其关键的非阻塞逻辑！）
float DS18B20_GetTemp(void) {
    uint8_t LSB, MSB;
    uint16_t temp_raw;
    float temp;

    if (DS18B20_Reset() == 0) {
        DS18B20_WriteByte(0xCC); 
        DS18B20_WriteByte(0xBE); 
        LSB = DS18B20_ReadByte();
        MSB = DS18B20_ReadByte();
        
        // 读完后立刻下达转换指令（减少在临界区停留时间）
        DS18B20_Reset();
        DS18B20_WriteByte(0xCC); 
        DS18B20_WriteByte(0x44); 

        temp_raw = (MSB << 8) | LSB;
        // 使用有符号强转，代码更简洁
        temp = (float)((int16_t)temp_raw) * 0.0625f;
        
    } else {
        temp = -99.9f; 
    }

    return temp - 2;
}
