#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "init.h"
#include "functions.h"

#define RELAY LATCbits.LATC0
#define BUZZER LATCbits.LATC1
#define SYS_LED LATCbits.LATC2
#define EMERG_LED LATCbits.LATC3

#define CONFIRM_BUTTON PORTBbits.RB0

volatile bool emergency_triggered = false;

uint8_t first_digit = 0;
uint8_t second_digit = 0;

uint8_t secret_digit1 = 2;
uint8_t secret_digit2 = 3;

#define DARK_THRESHOLD 400

void __interrupt(irq(IRQ_IOC), base(8)) IOC_ISR(void)
{
    if (IOCBFbits.IOCBF1)
    {
        IOCBFbits.IOCBF1 = 0;  // clear flag

        LATCbits.LATC1 = 1;    // LED2 ON
        __delay_ms(3000);
        LATCbits.LATC1 = 0;
    }
}

void main(void)
{
    init_system();

    SYS_LED = 1;
    display_digit(0);

    while (1)
    {
        if (emergency_triggered)
            emergency_reset();

        // FIRST DIGIT
        while (1)
        {
            if (read_adc(0) < DARK_THRESHOLD)
            {
                first_digit++;
                if (first_digit > 9) first_digit = 9;
                display_digit(first_digit);
                __delay_ms(300);
                while (read_adc(0) < DARK_THRESHOLD);
            }

            if (CONFIRM_BUTTON == 0)
            {
                __delay_ms(200);
                break;
            }
        }

        // SECOND DIGIT
        while (1)
        {
            if (read_adc(1) < DARK_THRESHOLD)
            {
                second_digit++;
                if (second_digit > 9) second_digit = 9;
                display_digit(second_digit);
                __delay_ms(300);
                while (read_adc(1) < DARK_THRESHOLD);
            }

            if (CONFIRM_BUTTON == 0)
            {
                __delay_ms(200);
                break;
            }
        }

        if ((first_digit == secret_digit1) &&
            (second_digit == secret_digit2))
        {
            RELAY = 1;
            __delay_ms(3000);
            RELAY = 0;
        }
        else
        {
            BUZZER = 1;
            __delay_ms(2000);
            BUZZER = 0;
        }

        first_digit = 0;
        second_digit = 0;
        display_digit(0);
    }
}
