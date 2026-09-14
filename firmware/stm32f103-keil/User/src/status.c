#include "status.h"
#include "main.h"

void calculateStatus(struct sensorData_t *p) {
	p->status = 0;

	// HR status
	if(p->ir_led > 30000) {  // wearing
		if(p->bpm > 120) {
			p->status |= 0x20; // HighHR
		} else if(p->bpm > 0 && p->bpm < 45) {
			p->status |= 0x10; // LowHR
		}
	} else { 
		p->bpm = 0; 
	}
	if ((p->status & 0x20) || (p->status & 0x10)) {			// if BPM too high or too low, triggers ALARM flag
		p->status |= 0x80;
	}
	
	// MPU, or activity level status
	if(p->activityLevel > 3000)      p->status |= 0x04; // Running
	else if(p->activityLevel > 500)  p->status |= 0x02; // Walking
	else                             p->status |= 0x01; // Sleeping

}
