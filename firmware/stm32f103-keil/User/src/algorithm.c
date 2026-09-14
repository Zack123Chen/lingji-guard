#include "algorithm.h"
#include "main.h"

typedef struct {
	float Q_hr;
	float Q_trend;
	float R_measure;
	float hr;
	float trend;
	float rate;
	float P[2][2];
} KalmanHR_t;

typedef struct {
	float Q_angle;
	float Q_bias;
	float R_measure;
	float angle;
	float bias;
	float rate;
	float P[2][2];
} KalmanMPU_t;

static KalmanMPU_t kalmanX = {0.001f, 0.003f, 0.03f, 0, 0, 0, {{0,0},{0,0}}};
static KalmanMPU_t kalmanY = {0.001f, 0.003f, 0.03f, 0, 0, 0, {{0,0},{0,0}}};
static KalmanHR_t k_hr_obj = {0.001f, 0.003f, 0.03f, 0, 0, 0, {{0,0},{0,0}}};

float KalmanHR_Update(KalmanHR_t *k, float newHR, float dt) {
	k->rate = 0 - k->trend;
	k->hr += dt * k->rate;

	k->P[0][0] += dt * (dt * k->P[1][1] - k->P[0][1] - k->P[1][0] + k->Q_hr);
	k->P[0][1] -= dt * k->P[1][1];
	k->P[1][0] -= dt * k->P[1][1];
	k->P[1][1] += k->Q_trend * dt;

	float S = k->P[0][0] + k->R_measure;
	float K[2];
	K[0] = k->P[0][0] / S;
	K[1] = k->P[1][0] / S;

	float y = newHR - k->hr;
	k->hr += K[0] * y;
	k->trend += K[1] * y;

	float P00_temp = k->P[0][0];
	float P01_temp = k->P[0][1];

	k->P[0][0] -= K[0] * P00_temp;
	k->P[0][1] -= K[0] * P01_temp;
	k->P[1][0] -= K[1] * P00_temp;
	k->P[1][1] -= K[1] * P01_temp;

	return k->hr;
}

void processHR(struct sensorData_t *p) {
	static uint32_t ir_avg = 100000;      
	static uint32_t last_beat_time = 0;   
	static float filtered_hr = 0;           
    
	int32_t ac_signal = 0;
	float raw_bpm = 0;

	ir_avg = (ir_avg * 31 + p->ir_led) / 32;
	ac_signal = abs((int32_t)p->ir_led - (int32_t)ir_avg);

	if (ac_signal > 600 && (p->timestamp - last_beat_time > 350)) {
		uint32_t delta_ms = p->timestamp - last_beat_time;
		last_beat_time = p->timestamp;

		raw_bpm = 60000.0f / delta_ms; 

		float dt = (float)delta_ms / 1000.0f;

		if (raw_bpm > 40 && raw_bpm < 200) {
			filtered_hr = KalmanHR_Update(&k_hr_obj, raw_bpm, dt);
		}
		
	}
	p->bpm = (uint8_t)(filtered_hr + 0.5f);
}

float KalmanMPU_Update(KalmanMPU_t *k, float newAngle, float newRate, float dt) {

	k->rate = newRate - k->bias;
	k->angle += dt * k->rate;

	k->P[0][0] += dt * (dt * k->P[1][1] - k->P[0][1] - k->P[1][0] + k->Q_angle);
	k->P[0][1] -= dt * k->P[1][1];
	k->P[1][0] -= dt * k->P[1][1];
	k->P[1][1] += k->Q_bias * dt;

	float S = k->P[0][0] + k->R_measure;
	float K[2] = { k->P[0][0] / S, k->P[1][0] / S };

	float y = newAngle - k->angle;
	k->angle += K[0] * y;
	k->bias  += K[1] * y;

	float P00_temp = k->P[0][0];
	float P01_temp = k->P[0][1];
	k->P[0][0] -= K[0] * P00_temp;
	k->P[0][1] -= K[0] * P01_temp;
	k->P[1][0] -= K[1] * P00_temp;
	k->P[1][1] -= K[1] * P01_temp;

	return k->angle;
}

void processMPU(int current_idx, int prev_idx) {
	struct sensorData_t *curr = &SystemData.dataQueue[current_idx];
	struct sensorData_t *prev = &SystemData.dataQueue[prev_idx];

	curr->activityLevel = abs(curr->mpu_data.AccX - prev->mpu_data.AccX) + 
                        abs(curr->mpu_data.AccY - prev->mpu_data.AccY) + 
                        abs(curr->mpu_data.AccZ - prev->mpu_data.AccZ);

	float dt = (float)(curr->timestamp - prev->timestamp) / 1000.0f;
	if (dt <= 0) dt = 0.01f;

	float accAngleX = atan2f((float)curr->mpu_data.AccY, (float)curr->mpu_data.AccZ) * 57.2957f;
	float gyroRateX = (float)curr->mpu_data.GyroX; 
	curr->mpu_data.roll = KalmanMPU_Update(&kalmanX, accAngleX, gyroRateX, dt);
	
	float accAngleY = atan2f(-(float)curr->mpu_data.AccX, (float)curr->mpu_data.AccZ) * 57.2957f;
	float gyroRateY = (float)curr->mpu_data.GyroY;
	curr->mpu_data.pitch = KalmanMPU_Update(&kalmanY, accAngleY, gyroRateY, dt);
}

void vLogicTask(void *pvParameters) {
	TickType_t xLastWakeTime = xTaskGetTickCount();
	const TickType_t xFrequency = pdMS_TO_TICKS(200);
    
	for(;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		uint8_t idx = SystemData.calc_idx;
		if (idx != SystemData.write_idx) {
			struct sensorData_t *p = &SystemData.dataQueue[idx];
			
			processHR(p);
			processMPU(idx,(idx+7) % 8);
			calculateStatus(p);
			
			SystemData.calc_idx = (idx + 1) % QUEUE_SIZE;
		}
	}
}
