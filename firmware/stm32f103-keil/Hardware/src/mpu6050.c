#include "mpu6050.h"
#include "Delay.h"

// 宏定义底层引脚 (PB6 和 PB7)
#define MPU_SCL_PORT GPIOB
#define MPU_SCL_PIN  GPIO_Pin_6
#define MPU_SDA_PORT GPIOB
#define MPU_SDA_PIN  GPIO_Pin_7

#define MPU_SCL_H()  GPIO_SetBits(MPU_SCL_PORT, MPU_SCL_PIN)
#define MPU_SCL_L()  GPIO_ResetBits(MPU_SCL_PORT, MPU_SCL_PIN)
#define MPU_SDA_H()  GPIO_SetBits(MPU_SDA_PORT, MPU_SDA_PIN)
#define MPU_SDA_L()  GPIO_ResetBits(MPU_SDA_PORT, MPU_SDA_PIN)
#define MPU_SDA_READ() GPIO_ReadInputDataBit(MPU_SDA_PORT, MPU_SDA_PIN)

static void MPU_I2C_Delay(void) {  }

static void MPU_I2C_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = MPU_SCL_PIN | MPU_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    MPU_SCL_H(); MPU_SDA_H();
}

static void MPU_I2C_Start(void) {
    MPU_SDA_H(); MPU_SCL_H(); MPU_I2C_Delay();
    MPU_SDA_L(); MPU_I2C_Delay(); MPU_SCL_L();
}

static void MPU_I2C_Stop(void) {
    MPU_SDA_L(); MPU_SCL_L(); MPU_I2C_Delay();
    MPU_SCL_H(); MPU_I2C_Delay(); MPU_SDA_H();
}

static void MPU_I2C_SendByte(uint8_t byte) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (byte & (0x80 >> i)) MPU_SDA_H();
        else MPU_SDA_L();
        MPU_SCL_H(); MPU_I2C_Delay(); MPU_SCL_L(); MPU_I2C_Delay();
    }
    MPU_SDA_H(); // 释放SDA等待应答
    MPU_SCL_H(); MPU_I2C_Delay(); MPU_SCL_L(); MPU_I2C_Delay();
}

static uint8_t MPU_I2C_ReadByte(uint8_t ack) {
    uint8_t i, byte = 0;
    MPU_SDA_H(); 
    for (i = 0; i < 8; i++) {
        MPU_SCL_H(); MPU_I2C_Delay();
        if (MPU_SDA_READ()) byte |= (0x80 >> i);
        MPU_SCL_L(); MPU_I2C_Delay();
    }
    if (ack) MPU_SDA_L(); else MPU_SDA_H();
    MPU_SCL_H(); MPU_I2C_Delay(); MPU_SCL_L(); MPU_I2C_Delay();
    return byte;
}

// 往 MPU6050 写寄存器
static void MPU6050_WriteReg(uint8_t RegAddress, uint8_t Data) {
    MPU_I2C_Start();
    MPU_I2C_SendByte(0xD0); // 设备地址
    MPU_I2C_SendByte(RegAddress);
    MPU_I2C_SendByte(Data);
    MPU_I2C_Stop();
}

// 初始化 MPU6050
void MPU6050_Init(void) {
    MPU_I2C_Init();
    MPU6050_WriteReg(0x6B, 0x00); // 解除休眠状态
    MPU6050_WriteReg(0x19, 0x09); // 采样率分频
    MPU6050_WriteReg(0x1A, 0x06); // 低通滤波
    MPU6050_WriteReg(0x1C, 0x08); // 加速度计量程 +-4g
    MPU6050_WriteReg(0x1B, 0x18); // 陀螺仪量程 +-2000度/s
}

// 读取 6 个轴的 3D 数据
MPU_Data_t MPU6050_GetData(void) {
    MPU_Data_t data;
    uint8_t buf[14];
    
    MPU_I2C_Start();
    MPU_I2C_SendByte(0xD0);
    MPU_I2C_SendByte(0x3B); // 从加速度 X 轴寄存器开始读
    MPU_I2C_Start();
    MPU_I2C_SendByte(0xD1); // 读模式
    
    for(int i = 0; i < 13; i++) buf[i] = MPU_I2C_ReadByte(1); // 发送 ACK 继续读
    buf[13] = MPU_I2C_ReadByte(0); // 最后一个发 NACK 停止
    MPU_I2C_Stop();

    // 拼装高 8 位和低 8 位数据
    data.AccX = (buf[0] << 8) | buf[1];
    data.AccY = (buf[2] << 8) | buf[3];
    data.AccZ = (buf[4] << 8) | buf[5];
    data.GyroX = (buf[8] << 8) | buf[9];
    data.GyroY = (buf[10] << 8) | buf[11];
    data.GyroZ = (buf[12] << 8) | buf[13];

    return data;
}
