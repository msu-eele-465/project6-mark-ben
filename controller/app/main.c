#include <msp430.h>
#include "../src/temp_sensor.h"
#include "../src/keypad.h"
#include "../src/statusled.h"
#include "../src/i2c_master.h"
#include <string.h>
#include <stdint.h>

char keypad_input[4] = {};
volatile int input_index = 0;
volatile int send_i2c_update_flag = 0;
volatile float base_tp = 0.5;    // Default 1.0s

volatile uint32_t mode_start_time = 0;

volatile int peltier_mode = 0;       // 0 OFF
                                     // 1 HEAT
                                     // 2 COOL
                                     // 3 MATCH
volatile int led_mode = 0;

volatile uint8_t led_count = 0;

volatile uint8_t time_spent_update_flag = 0;
volatile uint8_t plant_temp_update_flag = 0;
volatile uint8_t ambient_temp_update_flag = 0;

volatile uint8_t time_count = 0;

void setup_heartbeat() {
    // --    LED   --
    
    P6DIR |= BIT6;                                                      // P6.6 as OUTPUT
    P6OUT |= BIT6;                                                      // Start LED off

    // -- Timer B0 --
    TB0R = 0;
    TB0CCTL0 = CCIE;                                                    // Enable Interrupt
    TB0CCR0 = 32820;                                                    // 1 sec timer
    TB0EX0 = TBIDEX__8;                                                 // D8
    TB0CTL = TBSSEL__SMCLK | MC__UP | ID__4;                            // Small clock, Up counter,  D4
    TB0CCTL0 &= ~CCIFG;
}

void setup_ledbar_update_timer() {
    TB1CTL = TBSSEL__ACLK | MC__UP | ID__4;                             // Use ACLK, up mode, divider 4
    TB1CCR0 = (int)((32000 * base_tp) / 4.0);                           // Set update interval based on base_tp
    TB1CCTL0 = CCIE;                                                    // Enable interrupt for TB1 CCR0
}

void rgb_timer_setup() {

    TB2R = 0;
    TB2CTL |= (TBSSEL__SMCLK | MC__UP);                       // Small clock, Up counter
    TB2CCR0 = 512;                                            // 1 sec timer
    TB2CCTL0 |= CCIE;                                         // Enable Interrupt
    TB2CCTL0 &= ~CCIFG;
}

uint8_t compute_ledbar() {
    uint8_t led_pins = 0;

    switch (led_mode) {
        case 1:
            if(led_count < 8) {
                led_count++;
            }
            led_pins = ((1u << led_count) - 1) << (8 - led_count);
            break;
        case 2:
            if (led_count < 8) {
                led_count++;
            }
            led_pins = (1u << led_count) - 1;
            break;
        default:
            led_count = 0;
            led_pins = 0;
            break; 
    }
    return led_pins;
}

void update_slave_ledbar() {
    volatile int ledbar_pattern = compute_ledbar();
    while(i2c_busy);
    i2c_write_led(ledbar_pattern);
}

void process_keypad() {
    char key = pressed_key();
    if (key == '\0') return;
// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- HEAT ---------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------
    if(key == 'A') {
        peltier_mode = 1;
        led_count = 0;
        led_mode = 1;
        mode_start_time = read_RTC();
    } 
// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- COOL ---------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------    
    else if (key == 'B') {
        peltier_mode = 2;
        led_count = 0;
        led_mode = 2;
        mode_start_time = read_RTC();
    } 
// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- MATCH --------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------    
    else if (key == 'C') {
        peltier_mode = 3;
    }



}

void process_flags(void) {
    if(send_i2c_update_flag)
        update_slave_ledbar();
    send_i2c_update_flag = 0;


    if(ambient_temp_update_flag) {
        ambient_temp_update_flag = 0;
        update_LCD_async(0xFF, (int) moving_average, 0xFF);
    }

    if(plant_temp_update_flag) {
        plant_temp_update_flag = 0;
        //update_LCD_async();
    }

    if(time_spent_update_flag) {
        time_spent_update_flag = 0;
    }
}

void update_LCD_async(int modeID, int temperature, int window_size) {
    if (state_variable != 0 && state_variable != 2) {
        update_LCD(modeID, temperature, window_size);
    }
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer
    
    P1DIR |= BIT0;
    P1OUT &= ~BIT0;
    i2c_master_setup();
    setup_keypad();
    setup_heartbeat();
    setup_ledbar_update_timer();
    rgb_timer_setup();
    setup_temp_timer();
    setup_ADC();

    send_buff = 0;
    ready_to_send = 0;

    PM5CTL0 &= ~LOCKLPM5;                   // Disable the GPIO power-on default high-impedance mode
                                            // to activate previously configured port settings
    UCB0CTLW0 &= ~UCSWRST;                // Take out of reset
    UCB0IE |= UCTXIE0;
    UCB0IE |= UCRXIE0;

    __enable_interrupt();

    while(1)
    {
        process_keypad();
        compute_temp();
        while(i2c_busy);
        process_flags();
    }
}

// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- INTERRUPTS ---------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------

#pragma vector=TIMER0_B0_VECTOR
__interrupt void Timer_B0_ISR(void) {
    TB0CCTL0 &= ~CCIFG;
    P6OUT ^= BIT6;
    time_spent_update_flag = 1;
    if(time_count) {
        plant_temp_update_flag = 1;
        ambient_temp_update_flag = 1;
        time_count = 0;
    } else if(!time_count) {
        time_count++;
    }
    
}

#pragma vector = TIMER1_B0_VECTOR
__interrupt void Timer_B1_ISR(void) {
    TB1CCTL0 &= ~CCIFG;
    send_i2c_update_flag = 1;
    TB1CCR0 = (int)((32768 * base_tp) / 4.0);
}

 #pragma vector=EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_ISR(void){
    P1OUT |= BIT0;
    int current = UCB0IV;
    switch(current) {
        case 0x18:  // TXIFG
            UCB0TXBUF = send_buff;
            i2c_busy = 0;
            break;
        default:
            break;
    }
}
  