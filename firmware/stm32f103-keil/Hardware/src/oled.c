#include "stm32f10x.h"
#include "OLED.h"
#include "OLED_Font.h"

void OLED_I2C_Delay(void)
{
    uint8_t i = 10;
    while(i--);
}

void OLED_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出，适合I2C
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_Init(OLED_SCL_PORT, &GPIO_InitStructure);
    
    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(0);
    OLED_W_SCL(0);
    OLED_I2C_Delay();
}

void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(1);
    OLED_I2C_Delay();
}

void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(Byte & (0x80 >> i));
        OLED_I2C_Delay();
        OLED_W_SCL(1);
        OLED_I2C_Delay();
        OLED_W_SCL(0);
    }
    OLED_W_SCL(1);    // 释放SDA线以读取ACK（此处简单跳过）
    OLED_I2C_Delay();
    OLED_W_SCL(0);
}

void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);        // 从机地址
    OLED_I2C_SendByte(0x00);        // 控制字节：命令
    OLED_I2C_SendByte(Command); 
    OLED_I2C_Stop();
}

void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);        // 从机地址
    OLED_I2C_SendByte(0x40);        // 控制字节：数据
    OLED_I2C_SendByte(Data);
    OLED_I2C_Stop();
}

void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    OLED_WriteCommand(0xB0 | Y);                    // 设置页地址 0~7
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    // 设置列高4位
    OLED_WriteCommand(0x00 | (X & 0x0F));           // 设置列低4位
}

void OLED_Clear(void)
{   
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for(i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{           
    uint8_t i;
    uint8_t charIndex = Char - ' ';
    // 上半部分
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[charIndex][i]);
    }
    // 下半部分
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for (i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[charIndex][i + 8]);
    }
}

void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--) Result *= X;
    return Result;
}

void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)                            
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

void OLED_Init(void)
{
    uint32_t i;
    for (i = 0; i < 100000; i++); // 上电等待，确保OLED供电稳定
    
    OLED_I2C_Init();            
    
    OLED_WriteCommand(0xAE);    // 关闭显示
    OLED_WriteCommand(0xD5);    // 时钟分频
    OLED_WriteCommand(0x80);
    OLED_WriteCommand(0xA8);    // 复用率
    OLED_WriteCommand(0x3F);
    OLED_WriteCommand(0xD3);    // 显示偏移
    OLED_WriteCommand(0x00);
    OLED_WriteCommand(0x40);    // 开始行
    OLED_WriteCommand(0xA1);    // 左右镜像切换 (0xA1/0xA0)
    OLED_WriteCommand(0xC8);    // 上下镜像切换 (0xC8/0xC0)
    OLED_WriteCommand(0xDA);    // COM引脚配置
    OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);    // 对比度
    OLED_WriteCommand(0xCF);
    OLED_WriteCommand(0xD9);    // 预充电周期
    OLED_WriteCommand(0xF1);
    OLED_WriteCommand(0xDB);    // VCOMH电压
    OLED_WriteCommand(0x30);
    OLED_WriteCommand(0xA4);    // 全显开启/关闭
    OLED_WriteCommand(0xA6);    // 正显/反显
    OLED_WriteCommand(0x8D);    // 充电泵
    OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);    // 开启显示
        
    OLED_Clear();
}
