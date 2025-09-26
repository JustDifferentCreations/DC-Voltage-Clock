// current code

#ifndef F_CPU
#define F_CPU 1000000UL // Assume 1 MHz clock frequency for _delay_ms() [18]
#endif

#include <avr/io.h>         // Provides definitions for I/O registers [18]
#include <stdint.h>    
#include <util/delay.h>     // For software delays, if needed [18]
#include <avr/interrupt.h>  // For global interrupt enable/disable [18]     // For integer types like uint8_t, uint16_t, uint32_t [18]
#include <stdlib.h>
#include <stdbool.h>
#include "USI_I2C_Master.h"
#include "DS3231.h"

// Global variable to store the time
volatile Time_t now = {12, 0};

// --- PWM Pin Definitions ---
// Based on ATtiny24A/44A/84A pinout:
// PB2 (Port B, Pin 2) has alternate function OC0A [18-20]
// PA7 (Port A, Pin 7) has alternate function OC0B [18, 21, 22]
// PA5 (Port A, Pin 5) has alternate function OC1B [15, 18, 23]
#define PWM_OC0A_PIN PB2    // OC0A is on Port B, Pin 2 [18, 19]
#define PWM_OC0B_PIN PA7    // OC0B is on Port A, Pin 7 [18, 21]
#define PWM_OC1B_PIN PA5    // OC1B is on Port A, Pin 5 [15, 18]

// --- Timer TOP Values ---
// Timer/Counter0 is an 8-bit timer, so its TOP is 0xFF (255) in Fast PWM Mode 3. [13, 19, 24]
#define TIMER0_PWM_TOP 255
// Timer/Counter1 is a 16-bit timer. For 10-bit Fast PWM (Mode 7), TOP is 0x03FF (1023). [19, 25, 26]
#define TIMER1_PWM_TOP 1023

// --- Button Pin Definitions (Port B) ---
#define HOUR_BUTTON PB0 // Button to add 1 hour [15]
#define MINUTE_BUTTON PB1 // Button to add 1 minute [15]

// --- PWM Initialization Function ---
void pwm_init() {
    // --- Configure Timer/Counter0 for OC0A (PB2) and OC0B (PA7) ---
    // Timer/Counter0 is an 8-bit Timer/Counter module with two independent Output Compare Units (OC0A and OC0B) and PWM support [13, 42, 43].

    // 1. Set the OC0A pin (PB2) as an output. OC0A is on PB2. [20, 24]
    // The Data Direction Register (DDRB) bit for PB2 must be set to '1' for the output to be visible [24, 36, 44, 45].
    DDRB |= (1 << PWM_OC0A_PIN);

    // 2. Set the OC0B pin (PA7) and OC1B pin (PA5) as outputs. OC0B is on PA7, OC1B is on PA5. [22-24]
    // The Data Direction Register (DDRA) bit for PA7 and PA5 must be set to '1' for the output to be visible [24, 36, 44, 45].
    DDRA |= (1 << PWM_OC0B_PIN) | (1 << PWM_OC1B_PIN); // As per source [24]

    // 3. Set Timer/Counter0 to Fast PWM mode with TOP = 0xFF (Mode 3). [24]
    // This is achieved by setting WGM01 and WGM00 in TCCR0A and ensuring WGM02 in TCCR0B is cleared [24, 25, 46, 47].
    TCCR0A |= (1 << WGM01) | (1 << WGM00); // WGM01 and WGM00 for Mode 3 Fast PWM [25]
    TCCR0B &= ~(1 << WGM02);               // Ensure WGM02 is 0 for Mode 3 Fast PWM [25, 48]

    // 4. Configure Output Compare A (OC0A) for non-inverting Fast PWM. [25, 49]
    // OC0A output is cleared on Compare Match and set at BOTTOM. [25, 49, 50]
    // This corresponds to COM0A[1:0] = 10 (binary) in Table 11-3 [25, 49, 50].
    TCCR0A |= (1 << COM0A1);  // Set COM0A1 [25, 49, 50]
    TCCR0A &= ~(1 << COM0A0); // Clear COM0A0 [25, 49, 50]

    // 5. Configure Output Compare B (OC0B) for non-inverting Fast PWM. [26, 51]
    // OC0B output is cleared on Compare Match and set at BOTTOM. [26, 51, 52]
    // This corresponds to COM0B[1:0] = 10 (binary) in Table 11-6 [26, 51, 52].
    TCCR0A |= (1 << COM0B1); // Set COM0B1 [26, 51, 52]
    TCCR0A &= ~(1 << COM0B0); // Clear COM0B0 [26, 51, 52]

    // 6. Set initial duty cycles for OC0A and OC0B to 0. [26, 53]
    OCR0A = 0; // Initialize OC0A duty cycle [26]
    OCR0B = 0; // Initialize OC0B duty cycle [26]

    // 7. Select clock source for Timer/Counter0 (e.g., clkI/O/8). [44, 54]
    // This sets CS01 in TCCR0B, as per Table 11-9 (Description: clkI/O/8) [44, 54, 55].
    TCCR0B |= (1 << CS01); // Set CS01 for /8 prescaler [44, 54]
    TCCR0B &= ~((1 << CS02) | (1 << CS00)); // Clear CS02 and CS00 for /8 prescaler [44, 54]

    // --- Configure Timer/Counter1 for OC1B (PA5) ---
    // Timer/Counter1 is a 16-bit Timer/Counter module with two independent Output Compare Units (OC1A and OC1B) and PWM support. [42, 44, 56]
    // 1. Set the OC1B pin (PA5) as an output. [23, 44, 45]
    // The Data Direction Register (DDRA) bit for PA5 must be set to '1' for the output to be visible. [36, 45]
    // This was already done in step 2 of Timer/Counter0 setup: `DDRA |= (1 << PWM_OC0B_PIN) | (1 << PWM_OC1B_PIN);`
    // So no explicit line needed here.

    // 2. Set Timer/Counter1 to Fast PWM mode with TOP = 0x03FF (10-bit resolution, Mode 7). [45, 49, 57]
    // This is achieved by setting WGM11 and WGM10 in TCCR1A, and WGM12 in TCCR1B,
    // while ensuring WGM13 is cleared. [45, 49, 57]
    TCCR1A |= (1 << WGM11) | (1 << WGM10); // WGM11 and WGM10 for 10-bit Fast PWM [45]
    TCCR1B |= (1 << WGM12);                // WGM12 for 10-bit Fast PWM [45]
    TCCR1B &= ~(1 << WGM13);               // CRITICAL CORRECTION: Ensure WGM13 is 0 for 10-bit resolution (Mode 7) [45, 49, 57]

    // 3. Configure Output Compare B (OC1B) for non-inverting Fast PWM. [49, 51, 53]
    // OC1B output is cleared on Compare Match and set at BOTTOM.
    // This corresponds to COM1B[1:0] = 10 (binary) in Table 12-3. [49, 51, 53, 58]
    TCCR1A |= (1 << COM1B1); // Set COM1B1 [49, 51, 53, 58]
    TCCR1A &= ~(1 << COM1B0); // Clear COM1B0 [49, 51, 53, 58]

    // 4. Set initial duty cycle for OC1B to 0. [49, 51]
    OCR1B = 0; // Initialize OC1B duty cycle [49, 51]

    // 5. Select clock source for Timer/Counter1 (e.g., clkI/O/8). [51, 54, 59]
    // This sets CS11 in TCCR1B, as per Table 12-6 (Description: clkI/O/8). [51, 54, 59, 60]
    TCCR1B |= (1 << CS11); // Set CS11 for /8 prescaler [51, 54, 59]
    TCCR1B &= ~((1 << CS12) | (1 << CS10)); // Clear CS12 and CS10 for /8 prescaler [51, 54, 59]
}

// This function sets the duty cycle for OC0A (mapped to hours).
void set_pwm_HOUR_duty_cycle(void) {

    // Scales the 'hours' (0-23) to the full 8-bit PWM range (0-255).
    // The previous code `hours*10` would only map 0-10 hours to 0-100%, and cap at 100%.
    // This revised scaling provides a more even distribution for a 24-hour display.
    uint16_t ocr_val_calc = (((uint16_t)now.hh * (TIMER0_PWM_TOP + 1)) / 10);

    // Adjust for the "-1" in the formula to ensure 100% duty cycle reaches TOP [53].
    if (ocr_val_calc > 0) {
        OCR0A = (uint8_t)(ocr_val_calc - 1); // Update the 8-bit Output Compare Register [53]
    } else {
        OCR0A = 0;
    }
}

// This function sets the duty cycle for OC0B (mapped to minutes tens).
void set_pwm_MINUTES_T_duty_cycle(void) {
    // Scales the tens digit of 'minutes' (0-5) to the full 8-bit PWM range (0-255).
    // For example, 0-9 min = 0 tens, 10-19 min = 1 tens, ..., 50-59 min = 5 tens.
    uint8_t minutes_tens = now.mm / 10;
    // Scale for 6 possible values (0 to 5 tens) over the 8-bit PWM range (0-255).
    uint16_t ocr_val_calc = ((uint16_t)minutes_tens * (TIMER0_PWM_TOP + 1)) / 10;

    // Adjust for the "-1" in the formula to ensure 100% duty cycle reaches TOP [53, 54].
    if (ocr_val_calc > 0) {
        OCR0B = (uint8_t)(ocr_val_calc - 1); // Updated to OCR0B and cast to uint8_t [54]
    } else {
        OCR0B = 0; // Updated to OCR0B [59]
    }
}

// This function sets the duty cycle for OC1B (mapped to minutes units).
void set_pwm_MINUTES_duty_cycle(void) {
    // Scales the units digit of 'minutes' (0-9) to the full 10-bit PWM range (0-1023).
    // For example, 0-9 for units.
    uint8_t minutes_units = now.mm % 10;
    // Scale for 10 possible values (0 to 9 units) over the 10-bit PWM range (0-1023).
    uint32_t ocr_val_calc = ((uint32_t)minutes_units * (TIMER1_PWM_TOP + 1)) / 10;

    if (ocr_val_calc > 0) {
        OCR1B = (uint16_t)(ocr_val_calc - 1); // Update the 16-bit Output Compare Register [61]
    } else {
        OCR1B = 0;
    }
}

// Reads time from DS3231 and updates PWM outputs for display.
void update_and_display_time(void){
    // Read current RTC time and update global variables `hours`, `minutes`
    now = RTC_get_time(); //update time
    set_pwm_HOUR_duty_cycle();
    set_pwm_MINUTES_T_duty_cycle();
    set_pwm_MINUTES_duty_cycle();
}

// --- Main Program ---
int main() {
    // --- System Initialization ---



    // Configure button pins (PB0, PB1) as inputs with internal pull-ups. [36, 62, 63]
    // DDRB &= ~((1 << PB0) | (1 << PB1)); // Clear DDB0 and DDB1 bits to configure as inputs [36, 62]
    DDRB &= ~((1 << HOUR_BUTTON) | (1 << MINUTE_BUTTON)); 
    // PORTB |= ((1 << PB0) | (1 << PB1)); // Set PORTB0 and PORTB1 bits to enable pull-ups [36, 62]
    PORTB |= ((1 << HOUR_BUTTON) | (1 << MINUTE_BUTTON)); 

    // Initialize all PWM channels
    pwm_init();
    
    // initialize i2c
    I2C_init();
    
    update_and_display_time();
    // Enable L293Ds after time pwm and current pwm setting started
    DDRA |= (1 << PA0);   // Set PA0 as output
    PORTA |= (1 << PA0);  // Set PA0 high (enable L293Ds)
    DDRA |= (1 << PA1);   // Set PA1 as output [62]
    PORTA |= (1 << PA1);  // Set PA1 high [62]
    DDRA |= (1 << PA2);   // Set PA2 as output [62]
    PORTA |= (1 << PA2);  // Set PA2 high [62]
    
    
    // Initial delay for system stabilization [62]
    _delay_ms(100); 

    // --- Main Loop ---
    while (1) {
        // Continuously read time from DS3231 and update PWM displays
        update_and_display_time(); 
        for (int i = 0; i < 10; i++){
            // Button polling with debouncing for HOUR_BUTTON (PB0) [15]
            // Check if PB0 is low (button pressed) [64]
            if (!(PINB & (1 << HOUR_BUTTON))) { 
                _delay_ms(50); // Debounce delay [16]
                // Wait for button release to avoid multiple triggers from a single press [16]
                while (!(PINB & (1 << HOUR_BUTTON))); 
                _delay_ms(50); // Post-release debounce delay [16]
                // Increment hours, handling 24-hour rollover (0-23)
                now.hh = (now.hh + 1) % 24;
                // Write updated time to DS3231
                RTC_set_time(now);
                // Update display immediately after setting time
                update_and_display_time(); 
            }
            // Button polling with debouncing for MINUTE_BUTTON (PB1) [15]
            // Check if PB1 is low (button pressed) [17]
            if (!(PINB & (1 << MINUTE_BUTTON))) { 
                _delay_ms(50); // Debounce delay [17]
                // Wait for button release
                while (!(PINB & (1 << MINUTE_BUTTON))); 
                _delay_ms(50); // Post-release debounce
                // Increment minutes, handling 60-minute rollover (0-59)
                now.mm = (now.mm + 1) % 60;
                // Write updated time to DS3231
                RTC_set_time(now);
 
                // Update display immediately
                update_and_display_time(); 
            }
            _delay_ms(100); 
        }
    }
}