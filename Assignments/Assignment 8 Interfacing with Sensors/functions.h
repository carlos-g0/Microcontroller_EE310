#ifndef FUNCTIONS_
#define FUNCTIONS_H

#include <xc.h>
#include <stdint.h>

// Pin map for outputs
#define LED2   LATEbits.LATE1   // RE1 (emergency LED)
#define BUZZER LATCbits.LATC2   // RC2 (active buzzer)
#define RELAY  LATCbits.LATC3   // RC3 (relay input)

// 7-seg lookup table for digits 0–9 (common cathode)
static const uint8_t seg_lookup[10] =
{
    0b00111111, // 0
    0b00000110, // 1
    0b01011011, // 2
    0b01001111, // 3
    0b01100110, // 4
    0b01101101, // 5
    0b01111101, // 6
    0b00000111, // 7
    0b01111111, // 8
    0b01101111  // 9
};

// Show one digit (0–9) on the 7-seg
static inline void display_digit(uint8_t d)
{
    if (d < 10) LATD = seg_lookup[d];
}

// Read photoresistor #1 (PR1) on AN0 / RA0
static inline uint16_t adc_read_AN0(void)
{
    ADPCH = 0x00;          // select AN0
    __delay_us(20);        // small settle time
    ADCON0bits.GO = 1;     // start conversion
    while (ADCON0bits.GO); // wait until done
    return ((uint16_t)ADRESH << 8) | ADRESL;
}

// Read photoresistor #2 (PR2) on AN1 / RA1
static inline uint16_t adc_read_AN1(void)
{
    ADPCH = 0x01;          // select AN1
    __delay_us(20);
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);
    return ((uint16_t)ADRESH << 8) | ADRESL;
}

// Emergency melody pattern
static inline void emergency_melody(void)
{
    BUZZER = 1; __delay_ms(120); BUZZER = 0; __delay_ms(60);
    BUZZER = 1; __delay_ms(120); BUZZER = 0; __delay_ms(60);
    BUZZER = 1; __delay_ms(350); BUZZER = 0; __delay_ms(100);

    BUZZER = 1; __delay_ms(180); BUZZER = 0; __delay_ms(60);
    BUZZER = 1; __delay_ms(180); BUZZER = 0; __delay_ms(60);
}

#endif
