#include "msp430fr2355.h"
#include <msp430.h>
#include "i2c_master.h"
#include "temp_sensor.h"
#include <stdint.h>

volatile int i2c_busy = 0;

void i2c_master_setup(void) {
    //-- eUSCI_B0 --
    UCB0CTLW0 |= UCSWRST;

    UCB0CTLW0 |= UCSSEL__SMCLK;              // SMCLK
    UCB0BRW = 10;                       // Divider

    UCB0CTLW0 |= UCMODE_3;              // I2C Mode
    UCB0CTLW0 |= UCMST;                 // Master
    UCB0CTLW0 |= UCTR;                  // Tx
    UCB0CTLW1 |= UCASTP_2;
    //-- Configure GPIO --------
    P1SEL1 &= ~BIT3;           // eUSCI_B0
    P1SEL0 |= BIT3;

    P1SEL1 &= ~BIT2;
    P1SEL0 |= BIT2;

    
}

void update_LCD(unsigned int modeID, unsigned int tempAmbient, unsigned int tempPelt, unsigned int window_size, unsigned int timeSec) {
    if (modeID != 0x00FF) {
        update_LCD_raw(0, modeID);
    }

    if (tempAmbient != 0x00FF) {
        update_LCD_raw(1, tempAmbient);
    }

    if (tempPelt != 0x00FF) {
        update_LCD_raw(2, tempPelt);
    }

    if (window_size != 0x00FF) {
        update_LCD_raw(3, window_size);
    }

    if (timeSec != 0x00FF) {
        update_LCD_raw(4, timeSec);
    }
}

void update_LCD_raw(uint8_t opcode, unsigned int operand) {
    while(i2c_busy);
    i2c_busy = 1;

    UCB0IE &= ~UCTXIE0;

    UCB0CTLW0 |= UCTR;  // Transmit mode
    UCB0I2CSA = 0x02;   // Slave address

    UCB0TBCNT = 3;

    UCB0IFG &= ~UCSTPIFG;

    uint8_t lcdData[3];
    lcdData[0] = opcode;
    lcdData[1] = (uint8_t)((operand >> 8) & 0xFF);
    lcdData[2] = (uint8_t)(operand & 0xFF);

    UCB0CTLW0 |= UCTXSTT;
    int i;
    for (i = 0; i < 4; i++) {
        while(!(UCB0IFG & UCTXIFG));
        UCB0TXBUF = lcdData[i];
    }

     unsigned int timeout = 1000;
    while (!(UCB0IFG & UCSTPIFG) && timeout--) {
        ;
    }   
    UCB0IFG &= ~UCSTPIFG;

    i2c_busy = 0;

    UCB0IE |= UCTXIE0;
}

void i2c_write_led(unsigned int pattNum) {
    while(i2c_busy);
    i2c_busy = 1;

    UCB0CTLW0 |= UCTR;  // Transmit mode
    UCB0I2CSA = 0x40;   // Slave address

    UCB0TBCNT = 1;

    UCB0IFG &= ~UCSTPIFG;

    send_buff = pattNum;
    UCB0TXBUF = send_buff;

    UCB0CTLW0 |= UCTXSTT;

    UCB0IFG &= ~UCSTPIFG;

    i2c_busy = 0;

}

int i2c_read_lm92() {
    unsigned int high_byte;
    unsigned int low_byte;

    while (i2c_busy);
    i2c_busy = 1;

    UCB0CTLW0 &= ~UCTR; // Switch to receive
    UCB0I2CSA = 0b01001000; // Set slave address

    UCB0TBCNT = 2; // Set count

    UCB0CTLW0 |= UCTXSTT; // Generate start condition

    while (!(UCB0IFG & UCRXIFG));
    high_byte = UCB0RXBUF;

    while (!(UCB0IFG & UCRXIFG));
    low_byte = UCB0RXBUF;

    UCB0CTLW0 |= UCTR; // Switch to tx

    return (high_byte << 5) + (low_byte >> 3);
}
 
