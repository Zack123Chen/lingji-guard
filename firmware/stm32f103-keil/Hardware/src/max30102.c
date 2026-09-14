#include "max30102.h"
#include "main.h"

static uint8_t max30102_delay_count = 100;

static void MAX30102_Delay(void) {
    uint8_t i = max30102_delay_count;
    while(i--);
}

static void MAX30102_IIC_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_InitStructure.GPIO_Pin = IIC_SCL_PIN | IIC_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    IIC_SCL_H;
    IIC_SDA_H;
}

static void MAX30102_IIC_Start(void) {
    IIC_SDA_H; IIC_SCL_H; MAX30102_Delay();
    IIC_SDA_L; MAX30102_Delay(); IIC_SCL_L;
}

static void MAX30102_IIC_Stop(void) {
    IIC_SCL_L; IIC_SDA_L; MAX30102_Delay();
    IIC_SCL_H; MAX30102_Delay(); IIC_SDA_H; MAX30102_Delay();
}

static uint8_t MAX30102_IIC_Wait_Ack(void) {
    uint8_t errTime = 0;
    IIC_SDA_H; MAX30102_Delay(); IIC_SCL_H; MAX30102_Delay();
    while(IIC_SDA_READ) {
        errTime++;
        if(errTime > 250) { MAX30102_IIC_Stop(); return 1; }
    }
    IIC_SCL_L; return 0;  
}

static void MAX30102_IIC_Ack(void) {
    IIC_SCL_L; IIC_SDA_L; MAX30102_Delay();
    IIC_SCL_H; MAX30102_Delay(); IIC_SCL_L;
}

static void MAX30102_IIC_NAck(void) {
    IIC_SCL_L; IIC_SDA_H; MAX30102_Delay();
    IIC_SCL_H; MAX30102_Delay(); IIC_SCL_L;
}

static void MAX30102_IIC_Send_Byte(uint8_t txd) {                        
    uint8_t t;   
    IIC_SCL_L;
    for(t=0; t<8; t++) {              
        if((txd & 0x80) >> 7) IIC_SDA_H;
        else IIC_SDA_L;
        txd <<= 1;       
        MAX30102_Delay(); IIC_SCL_H; MAX30102_Delay(); IIC_SCL_L; MAX30102_Delay();
    }    
}

static uint8_t MAX30102_IIC_Read_Byte(uint8_t ack) {
    uint8_t i, receive = 0;
    IIC_SDA_H; 
    for(i=0; i<8; i++) {
        IIC_SCL_L; MAX30102_Delay(); IIC_SCL_H; MAX30102_Delay();
        receive <<= 1;
        if(IIC_SDA_READ) receive++;   
    }
    if (!ack) MAX30102_IIC_NAck();
    else MAX30102_IIC_Ack();
    return receive;
}

void MAX30102_WriteReg(uint8_t reg, uint8_t data) {
    static uint8_t initialized = 0;
    if(!initialized) {
        MAX30102_IIC_Init();
        initialized = 1;
    }
    
    MAX30102_IIC_Start();
    MAX30102_IIC_Send_Byte(0xAE);
    MAX30102_IIC_Wait_Ack();
    MAX30102_IIC_Send_Byte(reg);
    MAX30102_IIC_Wait_Ack();
    MAX30102_IIC_Send_Byte(data);
    MAX30102_IIC_Wait_Ack();
    MAX30102_IIC_Stop();
}

void MAX30102_Init(void) {
    MAX30102_WriteReg(0x09, 0x40);
    for(volatile int i = 0; i < 1000; i++);
    
    MAX30102_WriteReg(0x08, 0x27);
    MAX30102_WriteReg(0x09, 0x03);
    MAX30102_WriteReg(0x0A, 0x27);
    MAX30102_WriteReg(0x0C, 0x24);
    MAX30102_WriteReg(0x0D, 0x24);
    
    MAX30102_WriteReg(0x02, 0x00); 
    MAX30102_WriteReg(0x03, 0x00); 
    MAX30102_WriteReg(0x04, 0x00); 
}

void MAX30102_Read(uint32_t *pun_red_led, uint32_t *pun_ir_led) {
    uint32_t un_temp;
    uint8_t temp[6];
    
    MAX30102_IIC_Start();
    MAX30102_IIC_Send_Byte(0xAE);
    MAX30102_IIC_Wait_Ack();
    MAX30102_IIC_Send_Byte(0x07);
    MAX30102_IIC_Wait_Ack();
    
    MAX30102_IIC_Start();
    MAX30102_IIC_Send_Byte(0xAF);
    MAX30102_IIC_Wait_Ack();
    
    temp[0] = MAX30102_IIC_Read_Byte(1);
    temp[1] = MAX30102_IIC_Read_Byte(1);
    temp[2] = MAX30102_IIC_Read_Byte(1);
    temp[3] = MAX30102_IIC_Read_Byte(1);
    temp[4] = MAX30102_IIC_Read_Byte(1);
    temp[5] = MAX30102_IIC_Read_Byte(0);
    MAX30102_IIC_Stop();
    
    un_temp = temp[0];
    un_temp <<= 16;
    *pun_red_led = un_temp | (temp[1]<<8) | temp[2];
    *pun_red_led &= 0x03FFFF;
    
    un_temp = temp[3];
    un_temp <<= 16;
    *pun_ir_led = un_temp | (temp[4]<<8) | temp[5];
    *pun_ir_led &= 0x03FFFF;
}

/*
uint8_t max30102_Bus_Write(uint8_t Register_Address, uint8_t Word_Data)
{
	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_WR);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Send_Byte(Register_Address);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Send_Byte(Word_Data);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Stop();
	return 1;

cmd_fail:
	MAX30102_IIC_Stop();
	return 0;
}

uint8_t max30102_Bus_Read(uint8_t Register_Address)
{
	uint8_t data;

	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_WR);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Send_Byte((uint8_t)Register_Address);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_RD);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	data = MAX30102_IIC_Read_Byte(0);
	MAX30102_IIC_NAck();
	MAX30102_IIC_Stop();
	return data;

cmd_fail:
	MAX30102_IIC_Stop();
	return 0;
}

void max30102_FIFO_ReadWords(uint8_t Register_Address, u16 Word_Data[][2], uint8_t count)
{
	uint8_t i = 0;
	uint8_t no = count;
	uint8_t data1, data2;

	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_WR);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Send_Byte((uint8_t)Register_Address);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_RD);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}

	while (no)
	{
		data1 = MAX30102_IIC_Read_Byte(0);
		MAX30102_IIC_Ack();
		data2 = MAX30102_IIC_Read_Byte(0);
		MAX30102_IIC_Ack();
		Word_Data[i][0] = (((u16)data1 << 8) | data2);

		data1 = MAX30102_IIC_Read_Byte(0);
		MAX30102_IIC_Ack();
		data2 = MAX30102_IIC_Read_Byte(0);
		if (1 == no)
			MAX30102_IIC_NAck();
		else
			MAX30102_IIC_Ack();
		Word_Data[i][1] = (((u16)data1 << 8) | data2);

		no--;
		i++;
	}
	MAX30102_IIC_Stop();

cmd_fail:
	MAX30102_IIC_Stop();
}

void max30102_FIFO_ReadBytes(uint8_t Register_Address, uint8_t *Data)
{
	max30102_Bus_Read(REG_INTR_STATUS_1);
	max30102_Bus_Read(REG_INTR_STATUS_2);

	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_WR);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Send_Byte((uint8_t)Register_Address);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}
	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(max30102_WR_address | I2C_RD);
	if (MAX30102_IIC_Wait_Ack() != 0)
	{
		goto cmd_fail;
	}

	Data[0] = MAX30102_IIC_Read_Byte(1);
	Data[1] = MAX30102_IIC_Read_Byte(1);
	Data[2] = MAX30102_IIC_Read_Byte(1);
	Data[3] = MAX30102_IIC_Read_Byte(1);
	Data[4] = MAX30102_IIC_Read_Byte(1);
	Data[5] = MAX30102_IIC_Read_Byte(0);
	MAX30102_IIC_Stop();

cmd_fail:
	MAX30102_IIC_Stop();
}

void MAX30102_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	MAX30102_IIC_Init();
	MAX30102_Reset();

	max30102_Bus_Write(REG_INTR_ENABLE_1, 0xc0);
	max30102_Bus_Write(REG_INTR_ENABLE_2, 0x00);
	max30102_Bus_Write(REG_FIFO_WR_PTR, 0x00);
	max30102_Bus_Write(REG_OVF_COUNTER, 0x00);
	max30102_Bus_Write(REG_FIFO_RD_PTR, 0x00);
	max30102_Bus_Write(REG_FIFO_CONFIG, 0x0f);
	max30102_Bus_Write(REG_MODE_CONFIG, 0x03);
	max30102_Bus_Write(REG_SPO2_CONFIG, 0x27);
	max30102_Bus_Write(REG_LED1_PA, 0x24);
	max30102_Bus_Write(REG_LED2_PA, 0x24);
	max30102_Bus_Write(REG_PILOT_PA, 0x7f);
}

void MAX30102_Reset(void)
{
	max30102_Bus_Write(REG_MODE_CONFIG, 0x40);
	max30102_Bus_Write(REG_MODE_CONFIG, 0x40);
}

void MAX30102_IIC_SDA_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = MAX30102_IIC_SDA_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_Init(MAX30102_IIC_PORT, &GPIO_InitStructure);
}

void MAX30102_IIC_SDA_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Pin = MAX30102_IIC_SDA_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(MAX30102_IIC_PORT, &GPIO_InitStructure);
}

void MAX30102_IIC_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

	GPIO_InitStructure.GPIO_Pin = MAX30102_IIC_SCL_PIN | MAX30102_IIC_SDA_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(MAX30102_IIC_PORT, &GPIO_InitStructure);

	MAX30102_IIC_SCL_H();
	MAX30102_IIC_SDA_H();
}

void MAX30102_IIC_Start(void)
{
	MAX30102_IIC_SDA_OUT();
	MAX30102_IIC_SDA_H();
	MAX30102_IIC_SCL_H();
	Delay_us(4);
	MAX30102_IIC_SDA_L();
	Delay_us(4);
	MAX30102_IIC_SCL_L();
}

void MAX30102_IIC_Stop(void)
{
	MAX30102_IIC_SDA_OUT();
	MAX30102_IIC_SCL_L();
	MAX30102_IIC_SDA_L();
	Delay_us(4);
	MAX30102_IIC_SCL_H();
	MAX30102_IIC_SDA_H();
	Delay_us(4);
}

uint8_t MAX30102_IIC_Wait_Ack(void)
{
	uint8_t ucErrTime = 0;

	MAX30102_IIC_SDA_IN();
	MAX30102_IIC_SDA_H();
	Delay_us(1);
	MAX30102_IIC_SCL_H();
	Delay_us(1);
	while (MAX30102_READ_SDA())
	{
		ucErrTime++;
		if (ucErrTime > 250)
		{
			MAX30102_IIC_Stop();
			return 1;
		}
	}
	MAX30102_IIC_SCL_L();
	return 0;
}

void MAX30102_IIC_Ack(void)
{
	MAX30102_IIC_SCL_L();
	MAX30102_IIC_SDA_OUT();
	MAX30102_IIC_SDA_L();
	Delay_us(2);
	MAX30102_IIC_SCL_H();
	Delay_us(2);
	MAX30102_IIC_SCL_L();
}

void MAX30102_IIC_NAck(void)
{
	MAX30102_IIC_SCL_L();
	MAX30102_IIC_SDA_OUT();
	MAX30102_IIC_SDA_H();
	Delay_us(2);
	MAX30102_IIC_SCL_H();
	Delay_us(2);
	MAX30102_IIC_SCL_L();
}

void MAX30102_IIC_Send_Byte(uint8_t txd)
{
	uint8_t t;

	MAX30102_IIC_SDA_OUT();
	MAX30102_IIC_SCL_L();
	for (t = 0; t < 8; t++)
	{
		if ((txd & 0x80) >> 7) MAX30102_IIC_SDA_H(); else MAX30102_IIC_SDA_L();
		txd <<= 1;
		Delay_us(2);
		MAX30102_IIC_SCL_H();
		Delay_us(2);
		MAX30102_IIC_SCL_L();
		Delay_us(2);
	}
}

uint8_t MAX30102_IIC_Read_Byte(unsigned char ack)
{
	unsigned char i, receive = 0;

	MAX30102_IIC_SDA_IN();
	for (i = 0; i < 8; i++)
	{
		MAX30102_IIC_SCL_L();
		Delay_us(2);
		MAX30102_IIC_SCL_H();
		receive <<= 1;
		if (MAX30102_READ_SDA())
			receive++;
		Delay_us(1);
	}
	if (!ack)
		MAX30102_IIC_NAck();
	else
		MAX30102_IIC_Ack();
	return receive;
}

void MAX30102_IIC_ReadBytes(uint8_t deviceAddr, uint8_t writeAddr, uint8_t *data, uint8_t dataLength)
{
	uint8_t i;

	MAX30102_IIC_Start();
	MAX30102_IIC_Send_Byte(deviceAddr);
	MAX30102_IIC_Wait_Ack();
	MAX30102_IIC_Send_Byte(writeAddr);
	MAX30102_IIC_Wait_Ack();
	MAX30102_IIC_Send_Byte(deviceAddr | 0X01);
	MAX30102_IIC_Wait_Ack();

	for (i = 0; i < dataLength - 1; i++)
	{
		data[i] = MAX30102_IIC_Read_Byte(1);
	}
	data[dataLength - 1] = MAX30102_IIC_Read_Byte(0);
	MAX30102_IIC_Stop();
	Delay_ms(10);
}

void maxim_max30102_read_fifo(uint32_t *pun_red_led, uint32_t *pun_ir_led)
{
	uint32_t un_temp;
	//unsigned char uch_temp;
	char ach_i2c_data[6];

	*pun_red_led = 0;
	*pun_ir_led = 0;

	//uch_temp = max30102_Bus_Read(REG_INTR_STATUS_1);
	//uch_temp = max30102_Bus_Read(REG_INTR_STATUS_2);

	MAX30102_IIC_ReadBytes(I2C_WRITE_ADDR, REG_FIFO_DATA, (uint8_t *)ach_i2c_data, 6);

	un_temp = (unsigned char)ach_i2c_data[0];
	un_temp <<= 16;
	*pun_red_led += un_temp;
	un_temp = (unsigned char)ach_i2c_data[1];
	un_temp <<= 8;
	*pun_red_led += un_temp;
	un_temp = (unsigned char)ach_i2c_data[2];
	*pun_red_led += un_temp;

	un_temp = (unsigned char)ach_i2c_data[3];
	un_temp <<= 16;
	*pun_ir_led += un_temp;
	un_temp = (unsigned char)ach_i2c_data[4];
	un_temp <<= 8;
	*pun_ir_led += un_temp;
	un_temp = (unsigned char)ach_i2c_data[5];
	*pun_ir_led += un_temp;

	*pun_red_led &= 0x03FFFF;
	*pun_ir_led &= 0x03FFFF;
}

const uint16_t auw_hamm[31] = {41, 276, 512, 276, 41};
const uint8_t uch_spo2_table[184] = {
	95, 95, 95, 96, 96, 96, 97, 97, 97, 97, 97, 98, 98, 98, 98, 98, 99, 99, 99, 99,
	99, 99, 99, 99, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100,
	100, 100, 100, 100, 99, 99, 99, 99, 99, 99, 99, 99, 98, 98, 98, 98, 98, 98, 97, 97,
	97, 97, 96, 96, 96, 96, 95, 95, 95, 94, 94, 94, 93, 93, 93, 92, 92, 92, 91, 91,
	90, 90, 89, 89, 89, 88, 88, 87, 87, 86, 86, 85, 85, 84, 84, 83, 82, 82, 81, 81,
	80, 80, 79, 78, 78, 77, 76, 76, 75, 74, 74, 73, 72, 72, 71, 70, 69, 69, 68, 67,
	66, 66, 65, 64, 63, 62, 62, 61, 60, 59, 58, 57, 56, 56, 55, 54, 53, 52, 51, 50,
	49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 31, 30, 29,
	28, 27, 26, 25, 23, 22, 21, 20, 19, 17, 16, 15, 14, 12, 11, 10, 9, 7, 6, 5,
	3, 2, 1
};

static int32_t an_dx[BUFFER_SIZE - MA4_SIZE];
static int32_t an_x[BUFFER_SIZE];
static int32_t an_y[BUFFER_SIZE];

void maxim_heart_rate_and_oxygen_saturation(uint32_t *pun_ir_buffer, int32_t n_ir_buffer_length, uint32_t *pun_red_buffer, int32_t *pn_spo2, int8_t *pch_spo2_valid,
											int32_t *pn_heart_rate, int8_t *pch_hr_valid)
{
	uint32_t un_ir_mean, un_only_once;
	int32_t k, n_i_ratio_count;
	int32_t i, s, m, n_exact_ir_valley_locs_count, n_middle_idx;
	int32_t n_th1, n_npks, n_c_min;
	int32_t an_ir_valley_locs[15];
	int32_t an_exact_ir_valley_locs[15];
	int32_t an_dx_peak_locs[15];
	int32_t n_peak_interval_sum;
	int32_t n_y_ac, n_x_ac;
	int32_t n_spo2_calc;
	int32_t n_y_dc_max, n_x_dc_max;
	int32_t n_y_dc_max_idx, n_x_dc_max_idx;
	int32_t an_ratio[5], n_ratio_average;
	int32_t n_nume, n_denom;

	un_ir_mean = 0;
	for (k = 0; k < n_ir_buffer_length; k++)
		un_ir_mean += pun_ir_buffer[k];
	un_ir_mean = un_ir_mean / n_ir_buffer_length;
	for (k = 0; k < n_ir_buffer_length; k++)
		an_x[k] = pun_ir_buffer[k] - un_ir_mean;

	for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++)
	{
		n_denom = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]);
		an_x[k] = n_denom / (int32_t)4;
	}

	for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 1; k++)
		an_dx[k] = (an_x[k + 1] - an_x[k]);

	for (k = 0; k < BUFFER_SIZE - MA4_SIZE - 2; k++)
	{
		an_dx[k] = (an_dx[k] + an_dx[k + 1]) / 2;
	}

	for (i = 0; i < BUFFER_SIZE - HAMMING_SIZE - MA4_SIZE - 2; i++)
	{
		s = 0;
		for (k = i; k < i + HAMMING_SIZE; k++)
		{
			s -= an_dx[k] * auw_hamm[k - i];
		}
		an_dx[i] = s / (int32_t)1146;
	}

	n_th1 = 0;
	for (k = 0; k < BUFFER_SIZE - HAMMING_SIZE; k++)
	{
		n_th1 += ((an_dx[k] > 0) ? an_dx[k] : ((int32_t)0 - an_dx[k]));
	}
	n_th1 = n_th1 / (BUFFER_SIZE - HAMMING_SIZE);

	maxim_find_peaks(an_dx_peak_locs, &n_npks, an_dx, BUFFER_SIZE - HAMMING_SIZE, n_th1, 8, 5);

	n_peak_interval_sum = 0;
	if (n_npks >= 2)
	{
		for (k = 1; k < n_npks; k++)
			n_peak_interval_sum += (an_dx_peak_locs[k] - an_dx_peak_locs[k - 1]);
		n_peak_interval_sum = n_peak_interval_sum / (n_npks - 1);
		*pn_heart_rate = (int32_t)(6000 / n_peak_interval_sum);
		*pch_hr_valid = 1;
	}
	else
	{
		*pn_heart_rate = -999;
		*pch_hr_valid = 0;
	}

	for (k = 0; k < n_npks; k++)
		an_ir_valley_locs[k] = an_dx_peak_locs[k] + HAMMING_SIZE / 2;

	for (k = 0; k < n_ir_buffer_length; k++)
	{
		an_x[k] = pun_ir_buffer[k];
		an_y[k] = pun_red_buffer[k];
	}

	n_exact_ir_valley_locs_count = 0;
	for (k = 0; k < n_npks; k++)
	{
		un_only_once = 1;
		m = an_ir_valley_locs[k];
		n_c_min = 16777216;
		if (m + 5 < BUFFER_SIZE - HAMMING_SIZE && m - 5 > 0)
		{
			for (i = m - 5; i < m + 5; i++)
				if (an_x[i] < n_c_min)
				{
					if (un_only_once > 0)
					{
						un_only_once = 0;
					}
					n_c_min = an_x[i];
					an_exact_ir_valley_locs[k] = i;
				}
			if (un_only_once == 0)
				n_exact_ir_valley_locs_count++;
		}
	}
	if (n_exact_ir_valley_locs_count < 2)
	{
		*pn_spo2 = -999;
		*pch_spo2_valid = 0;
		return;
	}

	for (k = 0; k < BUFFER_SIZE - MA4_SIZE; k++)
	{
		an_x[k] = (an_x[k] + an_x[k + 1] + an_x[k + 2] + an_x[k + 3]) / (int32_t)4;
		an_y[k] = (an_y[k] + an_y[k + 1] + an_y[k + 2] + an_y[k + 3]) / (int32_t)4;
	}

	n_ratio_average = 0;
	n_i_ratio_count = 0;

	for (k = 0; k < 5; k++)
		an_ratio[k] = 0;
	for (k = 0; k < n_exact_ir_valley_locs_count; k++)
	{
		if (an_exact_ir_valley_locs[k] > BUFFER_SIZE)
		{
			*pn_spo2 = -999;
			*pch_spo2_valid = 0;
			return;
		}
	}

	for (k = 0; k < n_exact_ir_valley_locs_count - 1; k++)
	{
		n_y_dc_max = -16777216;
		n_x_dc_max = -16777216;
		if (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k] > 10)
		{
			for (i = an_exact_ir_valley_locs[k]; i < an_exact_ir_valley_locs[k + 1]; i++)
			{
				if (an_x[i] > n_x_dc_max)
				{
					n_x_dc_max = an_x[i];
					n_x_dc_max_idx = i;
				}
				if (an_y[i] > n_y_dc_max)
				{
					n_y_dc_max = an_y[i];
					n_y_dc_max_idx = i;
				}
			}
			n_y_ac = (an_y[an_exact_ir_valley_locs[k + 1]] - an_y[an_exact_ir_valley_locs[k]]) * (n_y_dc_max_idx - an_exact_ir_valley_locs[k]);
			n_y_ac = an_y[an_exact_ir_valley_locs[k]] + n_y_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);

			n_y_ac = an_y[n_y_dc_max_idx] - n_y_ac;
			n_x_ac = (an_x[an_exact_ir_valley_locs[k + 1]] - an_x[an_exact_ir_valley_locs[k]]) * (n_x_dc_max_idx - an_exact_ir_valley_locs[k]);
			n_x_ac = an_x[an_exact_ir_valley_locs[k]] + n_x_ac / (an_exact_ir_valley_locs[k + 1] - an_exact_ir_valley_locs[k]);
			n_x_ac = an_x[n_y_dc_max_idx] - n_x_ac;
			n_nume = (n_y_ac * n_x_dc_max) >> 7;
			n_denom = (n_x_ac * n_y_dc_max) >> 7;
			if (n_denom > 0 && n_i_ratio_count < 5 && n_nume != 0)
			{
				an_ratio[n_i_ratio_count] = (n_nume * 20) / n_denom;
				n_i_ratio_count++;
			}
		}
	}

	maxim_sort_ascend(an_ratio, n_i_ratio_count);
	n_middle_idx = n_i_ratio_count / 2;

	if (n_middle_idx > 1)
		n_ratio_average = (an_ratio[n_middle_idx - 1] + an_ratio[n_middle_idx]) / 2;
	else
		n_ratio_average = an_ratio[n_middle_idx];

	if (n_ratio_average > 2 && n_ratio_average < 184)
	{
		n_spo2_calc = uch_spo2_table[n_ratio_average];
		*pn_spo2 = n_spo2_calc;
		*pch_spo2_valid = 1;
	}
	else
	{
		*pn_spo2 = -999;
		*pch_spo2_valid = 0;
	}
}

void maxim_find_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height, int32_t n_min_distance, int32_t n_max_num)
{
	maxim_peaks_above_min_height(pn_locs, pn_npks, pn_x, n_size, n_min_height);
	maxim_remove_close_peaks(pn_locs, pn_npks, pn_x, n_min_distance);
	*pn_npks = min(*pn_npks, n_max_num);
}

void maxim_peaks_above_min_height(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_size, int32_t n_min_height)
{
	int32_t i = 1, n_width;
	*pn_npks = 0;

	while (i < n_size - 1)
	{
		if (pn_x[i] > n_min_height && pn_x[i] > pn_x[i - 1])
		{
			n_width = 1;
			while (i + n_width < n_size && pn_x[i] == pn_x[i + n_width])
				n_width++;
			if (pn_x[i] > pn_x[i + n_width] && (*pn_npks) < 15)
			{
				pn_locs[(*pn_npks)++] = i;
				i += n_width + 1;
			}
			else
				i += n_width;
		}
		else
			i++;
	}
}

void maxim_remove_close_peaks(int32_t *pn_locs, int32_t *pn_npks, int32_t *pn_x, int32_t n_min_distance)
{
	int32_t i, j, n_old_npks, n_dist;

	maxim_sort_indices_descend(pn_x, pn_locs, *pn_npks);

	for (i = -1; i < *pn_npks; i++)
	{
		n_old_npks = *pn_npks;
		*pn_npks = i + 1;
		for (j = i + 1; j < n_old_npks; j++)
		{
			n_dist = pn_locs[j] - (i == -1 ? -1 : pn_locs[i]);
			if (n_dist > n_min_distance || n_dist < -n_min_distance)
				pn_locs[(*pn_npks)++] = pn_locs[j];
		}
	}

	maxim_sort_ascend(pn_locs, *pn_npks);
}

void maxim_sort_ascend(int32_t *pn_x, int32_t n_size)
{
	int32_t i, j, n_temp;
	for (i = 1; i < n_size; i++)
	{
		n_temp = pn_x[i];
		for (j = i; j > 0 && n_temp < pn_x[j - 1]; j--)
			pn_x[j] = pn_x[j - 1];
		pn_x[j] = n_temp;
	}
}

void maxim_sort_indices_descend(int32_t *pn_x, int32_t *pn_indx, int32_t n_size)
{
	int32_t i, j, n_temp;
	for (i = 1; i < n_size; i++)
	{
		n_temp = pn_indx[i];
		for (j = i; j > 0 && pn_x[n_temp] > pn_x[pn_indx[j - 1]]; j--)
			pn_indx[j] = pn_indx[j - 1];
		pn_indx[j] = n_temp;
	}
}
*/




