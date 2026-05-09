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

// Optional: LED control (graduate extension). Change pin if you want.
// This uses RB1 as a simple digital output.
#define LED_TRIS   TRISBbits.TRISB1
#define LED_LAT    LATBbits.LATB1

// -----------------------------------------------------
// Simple PRNG: 16-bit Galois LFSR (good enough for lab)
// -----------------------------------------------------
static uint16_t lfsr = 0xACE1u;

static void seed_prng_runtime(void)
{
    // Mix in timer registers if they exist/running (safe reads).
    // If timers aren't running, it still works (just less random).
    uint16_t mix = 0;

    mix ^= ((uint16_t)TMR0L << 8) ^ (uint16_t)TMR0H;
    mix ^= ((uint16_t)TMR1L << 8) ^ (uint16_t)TMR1H;

    if (mix == 0) mix = 0xBEEF;
    lfsr ^= mix;
    if (lfsr == 0) lfsr = 0xACE1u;
}

static uint16_t prng_next(void)
{
    // taps: 16,14,13,11 (0xB400)
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

/*
    Main application
*/
int main(void)
{
    SYSTEM_Initialize();

    // MCC sometimes already initializes UART2 inside SYSTEM_Initialize(),
    // but your generated main.c had this call, so we keep it.
    UART2_Initialize();

    // If you want interrupts, enable here (not required for this assignment)
    // INTERRUPT_GlobalInterruptEnable();

    // LED output (optional graduate extension)
    LED_TRIS = 0;
    LED_LAT  = 0;

    seed_prng_runtime();

    // Print a header so you know it connected
    printf("\r\nUART Random Number Demo\r\n");
    printf("Random 1-100 every 1 second\r\n");
    printf("Optional: send 'A' to LED ON, 'B' to LED OFF\r\n\r\n");

    while(1)
    {
        // Print random number once per second
        uint8_t r = rand_1_to_100();
        printf("%u\r\n", r);

        // Graduate extension: receive a character and control LED
        // NOTE: If your MCC UART2 driver uses different function names,
        //       tell me what autocomplete shows and I’ll adjust.
        if (UART2_IsRxReady())
        {
            char c = UART2_Read();

            if (c == 'A' || c == 'a')
            {
                LED_LAT = 1;
                printf("RX:%c -> LED ON\r\n", c);
            }
            else if (c == 'B' || c == 'b')
            {
                LED_LAT = 0;
                printf("RX:%c -> LED OFF\r\n", c);
            }
            else
            {
                printf("RX:%c (ignored)\r\n", c);
            }
        }

        __delay_ms(1000);
    }
}
