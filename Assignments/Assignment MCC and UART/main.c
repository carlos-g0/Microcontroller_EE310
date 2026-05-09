/*
 * MAIN Generated Driver File
 *
 * @file main.c
 *
 * @defgroup main MAIN
 *
 * @brief This is the generated driver implementation file for the MAIN driver.
 *
 * @version MAIN Driver Version 1.0.2
 *
 * @version Package Version: 3.1.2
 */
#include "mcc_generated_files/system/system.h"
#include <xc.h>
#include <stdint.h>
#include <stdio.h>

#define _XTAL_FREQ 4000000UL

// PRNG 
static uint16_t lfsr = 0xACE1u;

static void seed_prng_runtime(void)
{
    uint16_t mix = 0;

    mix ^= ((uint16_t)TMR0L << 8) ^ (uint16_t)TMR0H;
    mix ^= ((uint16_t)TMR1L << 8) ^ (uint16_t)TMR1H;

    if (mix == 0) mix = 0xBEEF;
    lfsr ^= mix;
    if (lfsr == 0) lfsr = 0xACE1u;
}

static uint16_t prng_next(void)
{
    uint16_t lsb = lfsr & 1u;
    lfsr >>= 1;
    if (lsb) lfsr ^= 0xB400u;
    if (lfsr == 0) lfsr = 0xACE1u;
    return lfsr;
}

static uint8_t rand_1_to_100(void)
{
    return (uint8_t)((prng_next() % 100u) + 1u);
}

// Main
int main(void)
{
    SYSTEM_Initialize();
    UART2_Initialize();

    seed_prng_runtime();

    while (1)
    {
        printf("%u\r\n", rand_1_to_100());
        __delay_ms(1000);
    }
}
