#ifndef INIT_H
#define INIT_H

#include <xc.h>
#include <stdint.h>

static inline void init_adc(void)
{
    ADREFbits.ADPREF = 0b00;  // Vref+ = VDD
    ADREFbits.ADNREF = 0;     // Vref- = VSS

    ADCLK = 0x3F;
    ADACQ = 0x1F;

    ADCON0bits.FM = 1;
    ADCON0bits.ADON = 1;
}

static inline void init_system(void)
{
    // Analog settings:
    // RA0/RA1 analog for PR1/PR2
    ANSELA = 0x03;

    // Everything else digital
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;
    ANSELE = 0x00;      // <-- important: make RE pins digital

    // Clear outputs first
    LATC = 0x00;
    LATD = 0x00;
    LATE = 0x00;

    // Directions
    TRISAbits.TRISA0 = 1;  // PR1
    TRISAbits.TRISA1 = 1;  // PR2

    TRISBbits.TRISB0 = 1;  // confirm
    TRISBbits.TRISB1 = 1;  // emergency

    TRISCbits.TRISC2 = 0;  // buzzer
    TRISCbits.TRISC3 = 0;  // relay

    TRISDbits.TRISD0 = 0;
    TRISDbits.TRISD1 = 0;
    TRISDbits.TRISD2 = 0;
    TRISDbits.TRISD3 = 0;
    TRISDbits.TRISD4 = 0;
    TRISDbits.TRISD5 = 0;
    TRISDbits.TRISD6 = 0;
    TRISDbits.TRISD7 = 0;

    TRISEbits.TRISE1 = 0;  // LED2 now on RE1

    // Ensure outputs OFF
    LATCbits.LATC2 = 0;
    LATCbits.LATC3 = 0;
    LATEbits.LATE1 = 0;

    // Pull-ups for buttons
    WPUBbits.WPUB0 = 1;    // RB0 pull-up
    WPUBbits.WPUB1 = 1;    // RB1 pull-up

    // IOC on RB1 falling edge
    IOCBFbits.IOCBF1 = 0;
    PIR0bits.IOCIF = 0;

    IOCBNbits.IOCBN1 = 1;
    IOCBPbits.IOCBP1 = 0;

    PIE0bits.IOCIE = 1;
    INTCON0bits.GIE = 1;

    init_adc();
}

#endif
