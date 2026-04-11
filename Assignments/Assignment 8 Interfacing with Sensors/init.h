#ifndef INIT_H
#define INIT_H

#include <xc.h>

#define _XTAL_FREQ 1000000

void init_adc(void);

void init_system(void)
{
    // ---------- Disable Analog Where Not Needed ----------
    ANSELA = 0x03;   // RA0 & RA1 analog
    ANSELB = 0x00;
    ANSELC = 0x00;
    ANSELD = 0x00;

    // ---------- Clear Output Latches FIRST ----------
    LATC = 0x00;     // RC1, RC2, RC3 = LOW
    LATD = 0x00;     // 7-seg OFF

    // ---------- Set Pin Directions ----------
    TRISA = 0x03;    // RA0 & RA1 inputs
    TRISB = 0x03;    // RB0 confirm, RB1 interrupt
    TRISC = 0x00;    // RC1, RC2, RC3 outputs
    TRISD = 0x00;    // 7-segment outputs

    // ---------- Enable Internal Pull-ups ----------
    WPUBbits.WPUB0 = 1;   // Confirm button
    WPUBbits.WPUB1 = 1;   // Emergency button

    // ---------- Interrupt-On-Change Setup for RB1 ----------
    IOCBNbits.IOCBN1 = 1; // Falling edge detect on RB1
    PIE0bits.IOCIE = 1;   // Enable IOC interrupt
    INTCON0bits.GIE = 1;  // Global interrupt enable

    // ---------- Initialize ADC ----------
    init_adc();
}

void init_adc(void)
{
    ADCON0bits.ADON = 1;   // Enable ADC
    ADCON0bits.FM = 1;     // Right justified
}

#endif
