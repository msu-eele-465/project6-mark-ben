#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H
#include <stdint.h>

extern volatile unsigned int window_size;
// — Ambient (LM19) moving average —
extern volatile float   ambient_buf[100];
extern volatile uint8_t ambient_index;
extern volatile uint8_t ambient_count;
extern volatile float   ambient_sum;
extern volatile float moving_average_ambient;
extern volatile uint8_t ambient_temp_update_flag;
// — Plant (LM92) moving average —
extern volatile float   plant_buf[100];
extern volatile uint8_t plant_index;
extern volatile uint8_t plant_count;
extern volatile float   plant_sum;
extern volatile float moving_average_plant;
extern volatile uint8_t plant_temp_update_flag;
// ADC sampling state
extern volatile float ADC_value;
extern volatile uint8_t  sample_ready;
extern volatile uint8_t plant_read_flag;


void setup_ADC(void);
void setup_temp_timer(void);
void compute_temp(void);
void push_plant(float sample);
void push_ambient(float sample);

#endif