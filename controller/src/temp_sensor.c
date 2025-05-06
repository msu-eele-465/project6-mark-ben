#include <msp430.h>
#include <math.h>
#include "../src/i2c_master.h"
#include <stdint.h>

const unsigned int window_size = 3;
// — Ambient (LM19) moving average —
volatile float   ambient_buf[3];
volatile uint8_t ambient_index = 0;
volatile uint8_t ambient_count = 0;
volatile float   ambient_sum   = 0;
volatile float moving_average_ambient      = 0;
volatile uint8_t ambient_temp_update_flag  = 0;
// — Plant (LM92) moving average —
volatile float   plant_buf[3];
volatile uint8_t plant_index   = 0;
volatile uint8_t plant_count   = 0;
volatile float   plant_sum     = 0;
volatile float moving_average_plant        = 0;
volatile uint8_t plant_temp_update_flag    = 0;
// ADC sampling state
volatile float ADC_value     = 0;
volatile uint8_t  sample_ready  = 0;
volatile uint8_t plant_read_flag = 0;

void setup_ADC() {

    P1SEL0 |= BIT1;
    P1SEL1 |= BIT1;

    ADCCTL0 &= ~ADCSHT;                                     // Clear ADCSHT
    ADCCTL0 |= ADCSHT_2;                                    // Conversion Cycles = 16
    ADCCTL0 |= ADCON;                                       // Turn ADC on
    
    ADCCTL1 |= ADCSSEL_2;                                   // SMCLK Clock source
    ADCCTL1 |= ADCSHP;                                      // Sample signal source = timer

    ADCCTL2 &= ~ADCRES;                                     // Clear ADCRES
    ADCCTL2 |= ADCRES_2;                                    // Resolution = 12 Bit

    ADCMCTL0 |= ADCINCH_1;                                  // ADC input channel = A1 (P1.1)

    ADCIE |= ADCIE0;                                        // Enable ADC Conv Complete IRQ

}

void setup_temp_timer() {
    TB3R = 0;
    TB3CTL |= (TBSSEL__ACLK | MC__UP);                     // Small clock, Up counter
    TB3CCR0 = 16000;                                          // 0.5 sec timer
    TB3CCTL0 |= CCIE;                                       // Enable Interrupt
    TB3CCTL0 &= ~CCIFG;
}
// Push a new ambient sample into the moving average
void push_ambient(float sample) {
    ambient_sum -= ambient_buf[ambient_index];
    ambient_buf[ambient_index] = sample;
    ambient_sum += sample;
    ambient_index = (ambient_index + 1) % window_size;
    if (ambient_count < window_size) {
        ambient_count++;
    } else {
        ambient_count = 0;
    }
    moving_average_ambient     = ambient_sum / window_size;
    ambient_temp_update_flag   = 1;
}

// Push a new plant sample into the moving_average
void push_plant(float sample) {
    plant_sum -= plant_buf[plant_index];
    plant_buf[plant_index] = sample;
    plant_sum += sample;
    plant_index = (plant_index + 1) % window_size;
    if (plant_count < window_size) {
        plant_count++;
    } else {
        plant_count = 0;
    }
    moving_average_plant       = plant_sum / window_size;
    plant_temp_update_flag     = 1;
}


void compute_temp() {
    if (sample_ready) {
        __disable_interrupt();
        sample_ready = 0;
        float voltage = ADC_value * 0.000805f;
        

        float numerator = 1.8639f - voltage;
        float denominator = 3.88f * .00001f;
        float add = 2.1962f * 1000000;
        float base = (numerator/denominator) + add;

        float square = sqrt(base);
        //square = powf(base,0.5f);
        float new_sample = (-1481.96f + square);
        new_sample = 10 * new_sample;
        push_ambient(new_sample);
        __enable_interrupt();
        
    }
    
}

#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void){    

    ADC_value = ADCMEM0;                                    // Read ADC value
    sample_ready = 1;
    
}

#pragma vector=TIMER3_B0_VECTOR
__interrupt void Timer3_B0_ISR(void) {
    TB3CCTL0 &= ~CCIFG;

    ADCCTL0 |= ADCENC | ADCSC;                              // Trigger new ADC conversion every 0.5 seconds
    plant_read_flag = 1;
}

