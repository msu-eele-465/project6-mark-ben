#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <msp430.h>


void i2c_master_setup(void);

void update_LCD(unsigned int modeID, unsigned int tempAmbient, unsigned int tempPelt, unsigned int window_size, unsigned int timeSec);
void i2c_write_led(unsigned int pattNum);
void i2c_write_lcd(unsigned int pattNum, char character);
volatile int send_buff;
volatile int ready_to_send;
extern volatile int i2c_busy;

#endif