//-----------------------------
// Title:    Assignment 8 - Lock Box
//------------------------------
// Purpose:  Enter a 2-digit secret code using two photoresistors.
//           PR1 (RA0) = first digit counter, PR2 (RA1) = second digit counter.
//           RB0 = confirm button (moves to next digit / confirm).
//           RB1 = emergency interrupt button (stops program and plays melody).
// Dependencies: config.h, init.h, functions.h
// Compiler/IDE: MPLAB X IDE v6.30 + XC8 v3.10
// Author:   Carlos Gonzalez
// Date:     9/5/2026
// Inputs:
//   RA0 (AN0) - Photoresistor #1 voltage divider
//   RA1 (AN1) - Photoresistor #2 voltage divider
//   RB0       - Confirm pushbutton (internal pull-up, active LOW)
//   RB1       - Emergency pushbutton (internal pull-up, active LOW, interrupt)
// Outputs:
//   PORTD     - 7-seg display (common cathode)
//   RC3       - Relay control signal
//   RC2       - Active buzzer
//   RE1       - Emergency LED
// Versions:
//   v1.0 - working version with ADC counting
//   v2.0 - emergency button implemented
//   v3.0 - final version
//------------------------------

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "init.h"
#include "functions.h"

// Buttons are active LOW
#define CONFIRM_BUTTON PORTBbits.RB0
#define EMERG_BUTTON   PORTBbits.RB1

// Set by the interrupt, checked in main loop
volatile bool emergency_triggered = false;

// Secret code
static const uint8_t secret_digit1 = 2;
static const uint8_t secret_digit2 = 3;

// Quick “auto threshold” based on current bright average for PR1
static inline uint16_t threshold_AN0(void)
{
    uint32_t sum = 0;
    for (uint8_t i=0;i<8;i++){ sum += adc_read_AN0(); __delay_ms(5); }
    uint16_t bright = (uint16_t)(sum/8);
    return (bright > 200) ? (uint16_t)(bright - 150) : 100;
}

// Quick “auto threshold” based on current bright average for PR2
static inline uint16_t threshold_AN1(void)
{
    uint32_t sum = 0;
    for (uint8_t i=0;i<8;i++){ sum += adc_read_AN1(); __delay_ms(5); }
    uint16_t bright = (uint16_t)(sum/8);
    return (bright > 200) ? (uint16_t)(bright - 150) : 100;
}

// Wait for RB0 press + release (debounce)
static inline void wait_confirm_press_release(void)
{
    while (CONFIRM_BUTTON == 1) {;}
    __delay_ms(30);
    while (CONFIRM_BUTTON == 0) {;}
    __delay_ms(30);
}

// Emergency interrupt
void __interrupt(irq(IRQ_IOC), base(8)) IOC_ISR(void)
{
    // Check the IOC flag and RB1 flag
    if (PIR0bits.IOCIF && IOCBFbits.IOCBF1)
    {
        // Clear flags first
        IOCBFbits.IOCBF1 = 0;
        PIR0bits.IOCIF = 0;

        // If button is actually pressed
        if (EMERG_BUTTON == 0)
        {
            // Safety: stop relay right away
            RELAY = 0;

            // Temporarily disable IOC so it doesn't re-trigger during melody
            PIE0bits.IOCIE = 0;

            // Emergency sound
            emergency_melody();
            BUZZER = 0;

            emergency_triggered = true;

            // Clear flags again and re-enable IOC
            IOCBFbits.IOCBF1 = 0;
            PIR0bits.IOCIF = 0;
            PIE0bits.IOCIE = 1;
        }
    }
}

int main(void)
{
    init_system();

    // Start safe
    RELAY = 0;
    BUZZER = 0;
    LED2 = 0;

    // Show zero on power up
    display_digit(0);

    while (1)
    {
        // If emergency happened, do the 3 second LED + reset behavior
        if (emergency_triggered)
        {
            RELAY = 0;
            BUZZER = 0;

            LED2 = 1;
            __delay_ms(3000);
            LED2 = 0;

            display_digit(0);
            emergency_triggered = false;
        }

        // Get thresholds for “covered” detection
        uint16_t TH0 = threshold_AN0();
        uint16_t TH1 = threshold_AN1();

        // First digit entry (PR1)
        uint8_t first_digit = 0;
        display_digit(0);

        while (1)
        {
            if (emergency_triggered) break;

            // Cover PR1 -> voltage drops -> ADC becomes smaller
            if (adc_read_AN0() < TH0)
            {
                if (first_digit < 9) first_digit++;
                display_digit(first_digit);

                // Delay + wait until uncovered (debounce)
                __delay_ms(250);
                while (adc_read_AN0() < TH0) {;}
                __delay_ms(100);
            }

            // Confirm moves to digit #2
            if (CONFIRM_BUTTON == 0)
            {
                wait_confirm_press_release();
                break;
            }
        }

        if (emergency_triggered) continue;

        // Second digit entry (PR2)
        uint8_t second_digit = 0;
        display_digit(0);

        while (1)
        {
            if (emergency_triggered) break;

            if (adc_read_AN1() < TH1)
            {
                if (second_digit < 9) second_digit++;
                display_digit(second_digit);

                __delay_ms(250);
                while (adc_read_AN1() < TH1) {;}
                __delay_ms(100);
            }

            // Confirm finishes code entry
            if (CONFIRM_BUTTON == 0)
            {
                wait_confirm_press_release();
                break;
            }
        }

        if (emergency_triggered) continue;

        // Verify code
        if (first_digit == secret_digit1 && second_digit == secret_digit2)
        {
            RELAY = 1;
            __delay_ms(3000);
            RELAY = 0;
        }
        else
        {
            BUZZER = 1;
            __delay_ms(2500);
            BUZZER = 0;
        }

        // Always reset display back to 0 after attempt
        display_digit(0);
        __delay_ms(200);
    }
}
