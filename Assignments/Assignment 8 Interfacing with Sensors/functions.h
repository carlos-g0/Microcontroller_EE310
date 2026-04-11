#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <xc.h>
#include <stdint.h>

extern volatile bool emergency_triggered;

const uint8_t seg_lookup[10] =
{
    0b00111111,
    0b00000110,
    0b01011011,
    0b01001111,
    0b01100110,
    0b01101101,
    0b01111101,
    0b00000111,
    0b01111111,
    0b01101111
};

uint16_t read_adc(uint8_t channel)
{
    ADPCH = channel;
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);
    return ((uint16_t)ADRESH << 8) | ADRESL;
}

void display_digit(uint8_t digit)
{
    if (digit < 10)
        LATD = seg_lookup[digit];
}

void emergency_reset(void)
{
    LATCbits.LATC0 = 0;  // relay off
    LATCbits.LATC1 = 0;  // buzzer off
    LATCbits.LATC3 = 1;  // emergency LED on

    __delay_ms(3000);

    LATCbits.LATC3 = 0;
    emergency_triggered = false;

    LATD = seg_lookup[0];
}

#endif
