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
    TB2CTL |= (TBSSEL__SMCLK | MC__UP);                                 // Small clock, Up counter
    TB2CCR0 = 512;                                                      // 1 sec timer
    TB2CCTL0 |= CCIE;                                                   // Enable Interrupt
    TB2CCTL0 &= ~CCIFG;
}

uint8_t compute_ledbar() {
    uint8_t led_pins = 0;

    switch (led_mode) {
        case 2:
            if(led_count < 8) {
                led_count++;
            }
            led_pins = ((1u << led_count) - 1) << (8 - led_count);
            break;
        case 1:
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
        led_mode = peltier_mode;
        send_i2c_update_flag = 1;
        //mode_start_time = read_RTC();
        update_LCD(1, 0xFF,0xFF,0xFF,mode_start_time);
    } 
// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- COOL ---------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------    
    else if (key == 'B') {
        peltier_mode = 2;
        led_count = 0;
        led_mode = peltier_mode;
        send_i2c_update_flag = 1;
        //mode_start_time = read_RTC();
        update_LCD(2, 0xFF,0xFF,0xFF,mode_start_time);
    } 
// ----------------------------------------------------------------------------------------------------------------------------------------
// ------------- MATCH --------------------------------------------------------------------------------------------------------------------
// ----------------------------------------------------------------------------------------------------------------------------------------    
    else if (key == 'C') {
        peltier_mode = 3;
        led_mode = 0;              
        led_count = 0;             // restart the fill
        send_i2c_update_flag = 1;  
        //mode_start_time = read_RTC();
        update_LCD(3, 0xFF,0xFF,0xFF,mode_start_time);
    }

    else if (key == 'D') {
        peltier_mode = 0;
        mode_start_time = 0;
        ambient_temp_update_flag = 0;
        plant_temp_update_flag = 0;
        led_mode = 0;
        plant_read_flag = 0;
        update_LCD(0xFE, 0xFE, 0xFE, 0xFE, 0xFE);
    }



}

void process_flags(void) {
    if(send_i2c_update_flag)
        update_slave_ledbar();
    send_i2c_update_flag = 0;


    if(ambient_temp_update_flag) {
        ambient_temp_update_flag = 0;
        update_LCD(0xFF, moving_average_ambient, 0xFF, 0xFF, 0xFF);
    }

    if(plant_temp_update_flag) {
        plant_temp_update_flag = 0;
        update_LCD(0xFF, 0xFF, moving_average_plant, 0xFF, 0xFF);
    }

    if(time_spent_update_flag) {
        time_spent_update_flag = 0;
        //update_LCD(0xFF,0xFF,0xFF,0xFF, read_RTC() - mode_start_time);
    }
}

void update_peltier_mode(void) {
    switch(peltier_mode) {
        case 0:
            P5OUT &= ~(BIT0|BIT1);
            break;
        case 1:                             // Heat mode
            P5OUT &= ~(BIT0|BIT1);
            P5OUT |= BIT1;
            led_mode = 1;
            break;
        case 2:                             // Cool mode
            P5OUT &= ~(BIT0|BIT1);
            P5OUT |= BIT0;
            led_mode = 2;
            break;
        case 3: {
            float dif = moving_average_ambient - moving_average_plant;
            float thresh = 0.1f;
            if (dif > thresh) {             // Plant too hot
                P5OUT &= ~(BIT0|BIT1);
                P5OUT |= BIT0;
                led_mode = 2;
                send_i2c_update_flag = 1;
            } else if (dif < -thresh) {     // Plant too cool
                P5OUT &= ~(BIT0|BIT1);
                P5OUT |= BIT1;
                led_mode = 1;        
                send_i2c_update_flag = 1;        
            } else {
                P5OUT &= ~(BIT0|BIT1);
                led_mode = 0;
            }
            send_i2c_update_flag = 1;
            break;
        }
        default:
            break;
    }
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;               // Stop watchdog timer
    
    P5DIR |= BIT0 | BIT1;
    P5OUT &= ~(BIT0 | BIT1);

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
        if (plant_read_flag) {
            plant_read_flag = 0;
            int raw = i2c_read_lm92();
            int8_t  msb = (raw >> 8) & 0xFF;
            uint8_t lsb = raw & 0xFF;
            float temp_c = msb + (lsb / 256.0f);
            push_plant(temp_c);
        }
        if(peltier_mode != 0) {
/*             if((read_RTC() - mode_start_time) >= 300) {
                peltier_mode = 0;
                mode_start_time = 0;
                ambient_temp_update_flag = 0;
                plant_temp_update_flag = 0;
                led_mode = 0;
                plant_read_flag = 0;
                update_LCD(0xFE, 0xFE, 0xFE, 0xFE, 0xFE);
            } */
            update_peltier_mode();
        }
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
  