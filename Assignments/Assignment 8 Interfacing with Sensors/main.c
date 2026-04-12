#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include "init.h"
#include "functions.h"

#define CONFIRM_BUTTON PORTBbits.RB0
#define EMERG_BUTTON   PORTBbits.RB1

volatile bool emergency_triggered = false;

static const uint8_t secret_digit1 = 2;
static const uint8_t secret_digit2 = 3;

static inline uint16_t threshold_AN0(void)
{
    uint32_t sum = 0;
    for (uint8_t i=0;i<8;i++){ sum += adc_read_AN0(); __delay_ms(5); }
    uint16_t bright = (uint16_t)(sum/8);
    return (bright > 200) ? (uint16_t)(bright - 150) : 100;
}

static inline uint16_t threshold_AN1(void)
{
    uint32_t sum = 0;
    for (uint8_t i=0;i<8;i++){ sum += adc_read_AN1(); __delay_ms(5); }
    uint16_t bright = (uint16_t)(sum/8);
    return (bright > 200) ? (uint16_t)(bright - 150) : 100;
}

static inline void wait_confirm_press_release(void)
{
    while (CONFIRM_BUTTON == 1) {;}
    __delay_ms(30);
    while (CONFIRM_BUTTON == 0) {;}
    __delay_ms(30);
}

void __interrupt(irq(IRQ_IOC), base(8)) IOC_ISR(void)
{
    if (PIR0bits.IOCIF && IOCBFbits.IOCBF1)
    {
        IOCBFbits.IOCBF1 = 0;
        PIR0bits.IOCIF = 0;

        if (EMERG_BUTTON == 0)
        {
            RELAY = 0;

            PIE0bits.IOCIE = 0;

            emergency_melody();
            BUZZER = 0;

            emergency_triggered = true;

            IOCBFbits.IOCBF1 = 0;
            PIR0bits.IOCIF = 0;
            PIE0bits.IOCIE = 1;
        }
    }
}

int main(void)
{
    init_system();

    RELAY = 0;
    BUZZER = 0;
    LED2 = 0;

    display_digit(0);

    while (1)
    {
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

        uint16_t TH0 = threshold_AN0();
        uint16_t TH1 = threshold_AN1();

        uint8_t first_digit = 0;
        display_digit(0);

        while (1)
        {
            if (emergency_triggered) break;

            if (adc_read_AN0() < TH0)
            {
                if (first_digit < 9) first_digit++;
                display_digit(first_digit);

                __delay_ms(250);
                while (adc_read_AN0() < TH0) {;}
                __delay_ms(100);
            }

            if (CONFIRM_BUTTON == 0)
            {
                wait_confirm_press_release();
                break;
            }
        }

        if (emergency_triggered) continue;

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

            if (CONFIRM_BUTTON == 0)
            {
                wait_confirm_press_release();
                break;
            }
        }

        if (emergency_triggered) continue;

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

        display_digit(0);
        __delay_ms(200);
    }
}
