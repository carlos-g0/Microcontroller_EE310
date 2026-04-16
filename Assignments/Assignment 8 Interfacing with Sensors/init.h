#ifndef INIT_H
#define INIT_H

#include <xc.h>
#include <stdint.h>

// Set up ADC so it can read the photoresistors on RA0/RA1
static inline void init_adc(void)
{
    ADREFbits.ADPREF = 0b00;  // use VDD as Vref+
    ADREFbits.ADNREF = 0;     // use VSS (GND) as Vref-

    // ADC timing
    ADCLK = 0x3F;
    ADACQ = 0x1F;

    ADCON0bits.FM = 1;
    ADCON0bits.ADON = 1;      // turn ADC on
}

// Set up all pins (inputs/outputs), pull-ups, and the interrupt
static inline void init_system(void)
{
    // RA0/RA1 are analog inputs for PR1/PR2
    ANSELA = 0x03;

    // Everything else is digital I/O
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELE = 0x00;

    // Start all outputs LOW
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;

    // Inputs
    TRISAbits.TRISA0 = 1;  // PR1 (AN0)
    TRISAbits.TRISA1 = 1;  // PR2 (AN1)
    TRISBbits.TRISB0 = 1;  // confirm button
    TRISBbits.TRISB1 = 1;  // emergency button

    // Outputs
    TRISCbits.TRISC2 = 0;  // buzzer
    TRISCbits.TRISC3 = 0;  // relay

    // 7-seg uses all of PORTD as outputs
    TRISDbits.TRISD0 = 0;
    TRISDbits.TRISD1 = 0;
    TRISDbits.TRISD2 = 0;
    TRISDbits.TRISD3 = 0;
    TRISDbits.TRISD4 = 0;
    TRISDbits.TRISD5 = 0;
    TRISDbits.TRISD6 = 0;
    TRISDbits.TRISD7 = 0;

    TRISEbits.TRISE1 = 0;  // LED2 output

    // Make sure these outputs are OFF at startup
    LATCbits.LATC2 = 0;
    LATCbits.LATC3 = 0;
    LATEbits.LATE1 = 0;

    // Enable internal pull-ups for buttons
    WPUBbits.WPUB0 = 1;    // RB0
    WPUBbits.WPUB1 = 1;    // RB1

    // Interrupt on change for emergency button RB1
    IOCBFbits.IOCBF1 = 0;  // clear old flag
    PIR0bits.IOCIF = 0;

    IOCBNbits.IOCBN1 = 1;  // falling edge enable
    IOCBPbits.IOCBP1 = 0;  // rising edge disabled

    PIE0bits.IOCIE = 1;    // enable IOC interrupt
    INTCON0bits.GIE = 1;   // global interrupts on

    init_adc();
}

#endif
