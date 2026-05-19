//------------------------------
// Title: Final Project - Closed Loop Reaction Time Gauge
//------------------------------
// Purpose:  Measure a user?s reaction time using a timed LED start sequence,
//           display the result on a 16x2 LCD, and move a servo driven gauge
//           pointer to the corresponding position. The final system is designed
//           as a closed-loop feedback system using ADC based position sensing
//           to improve pointer accuracy and repeatability.
// Dependencies: XC8 libraries
// Compiler/IDE: MPLAB X IDE + XC8
// Authors:   Carlos Gonzalez, Geovani Palomec
// Date:     4/24/2026
// Inputs:
//   RD2        - Start/Reaction pushbutton (active LOW)
//   RB1        - Reset pushbutton (active LOW, IOC interrupt)
//   ANA3/RA3   - Potentiometer feedback signal for ADC position sensing
// Outputs:
//   RC2        - LCD RS
//   RC3        - LCD E
//   RC4        - LCD D4
//   RC5        - LCD D5
//   RC6        - LCD D6
//   RD4        - LCD D7
//   RD0        - Buzzer
//   RB3        - Ready LED
//   RB0        - Reset/Interrupt LED
//   RA2        - Sequence LED 1
//   RA1        - Sequence LED 2
//   RB5        - Sequence LED 3
//   RB4        - Sequence LED 4
//   RC7        - Sequence LED 5
//   RA0        - Servo control output
// Features:
//   - F1 style LED countdown/start sequence
//   - Randomized lights-out delay
//   - Timer based reaction time measurement
//   - LCD result display
//   - Servo gauge output using Timer0 pulse generation
//   - Reset button interrupt handling
//   - ADC based feedback for closed-loop pointer positioning
// Versions:
//   v1.0 - LED sequence, reaction timing, LCD output working
//   v2.0 - Servo gauge mapping and reset behavior added
//   v3.0 - Closed-loop ADC feedback added for pointer correction
//------------------------------

#include "Config.h"
#include "Func.h"

// =====================================================
// HELPERS
// =====================================================

void all_seq_leds_off(void)
{
    L1_LAT = 0;
    L2_LAT = 0;
    L3_LAT = 0;
    L4_LAT = 0;
    L5_LAT = 0;
}

void buzzer_off(void)
{
    BUZZ_LAT = 0;
}

void buzzer_on(void)
{
    BUZZ_LAT = 1;
}

void make_line16(char out[17], const char *in)
{
    for (uint8_t i = 0; i < 16; i++)
        out[i] = ' ';

    out[16] = '\0';

    for (uint8_t i = 0; i < 16 && in[i] != '\0'; i++)
        out[i] = in[i];
}

// =====================================================
// LCD
// =====================================================

void lcd_pulse_enable(void)
{
    LCD_E_LAT = 1;
    __delay_us(2);
    LCD_E_LAT = 0;
    __delay_us(50);
}

void lcd_write4(uint8_t n)
{
    LCD_D4_LAT = (n >> 0) & 1;
    LCD_D5_LAT = (n >> 1) & 1;
    LCD_D6_LAT = (n >> 2) & 1;
    LCD_D7_LAT = (n >> 3) & 1;

    lcd_pulse_enable();
}

void lcd_cmd(uint8_t c)
{
    LCD_RS_LAT = 0;

    lcd_write4(c >> 4);
    lcd_write4(c & 0x0F);

    __delay_ms(2);
}

void lcd_data(uint8_t d)
{
    LCD_RS_LAT = 1;

    lcd_write4(d >> 4);
    lcd_write4(d & 0x0F);

    __delay_us(60);
}

void lcd_goto(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 1) ? (0x80 + col) : (0xC0 + col);
    lcd_cmd(addr);
}

void lcd_print_16(const char *s16)
{
    for (uint8_t i = 0; i < 16; i++)
        lcd_data((uint8_t) s16[i]);
}

void lcd_init(void)
{
    LCD_RS_TRIS = 0;
    LCD_E_TRIS = 0;
    LCD_D4_TRIS = 0;
    LCD_D5_TRIS = 0;
    LCD_D6_TRIS = 0;
    LCD_D7_TRIS = 0;

    LCD_RS_LAT = 0;
    LCD_E_LAT = 0;

    __delay_ms(20);

    lcd_write4(0x03);
    __delay_ms(5);

    lcd_write4(0x03);
    __delay_us(150);

    lcd_write4(0x03);
    __delay_us(150);

    lcd_write4(0x02);

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);

    __delay_ms(2);
}

// =====================================================
// TIMER1
// =====================================================

void tmr1_init_1us_freerun(void)
{
    T1CONbits.ON = 0;

    T1CLK = 0x01;
    T1CONbits.CKPS = 0b00;

    TMR1H = 0;
    TMR1L = 0;

    T1CONbits.ON = 1;
}

void tmr1_zero(void)
{
    TMR1H = 0;
    TMR1L = 0;
}

// =====================================================
// ADC
// =====================================================

void adc_init(void)
{
    SERVO_FB_TRIS = 1;

    ADCLK = 0x3F;

    ADREF = 0x00;

    ADCON0bits.FM = 1;
    ADCON0bits.CS = 1;

    ADCON0bits.ON = 1;
}

uint16_t adc_read_an3(void)
{
    ADPCH = 0x03;

    __delay_us(5);

    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);

    return (((uint16_t) ADRESH << 8) | ADRESL);
}

// =====================================================
// TIMER0 SERVO DRIVER
// =====================================================

void tmr0_reload_us(uint16_t us)
{
    uint16_t preload = (uint16_t) (65536u - us);

    TMR0H = (uint8_t) (preload >> 8);
    TMR0L = (uint8_t) (preload & 0xFF);
}

void servo_timer0_init(void)
{
    T0CON0bits.EN = 0;
    T0CON0bits.MD16 = 1;
    T0CON0bits.OUTPS = 0;

    T0CON1bits.CS = 0b010;
    T0CON1bits.CKPS = 0b0000;
    T0CON1bits.ASYNC = 0;

    SERVO_LAT = 0;
    servo_phase_hi = 0;

    tmr0_reload_us(1000);

    PIR3bits.TMR0IF = 0;
    PIE3bits.TMR0IE = 1;

    T0CON0bits.EN = 1;
}

void __interrupt(irq(IRQ_TMR0), base(0x4008)) TMR0_ISR(void)
{
    PIR3bits.TMR0IF = 0;

    uint16_t pulse = servo_pulse_us;

    if (pulse < SERVO_US_ZERO)
        pulse = SERVO_US_ZERO;

    if (pulse > SERVO_US_MAX)
        pulse = SERVO_US_MAX;

    T0CON0bits.EN = 0;

    if (servo_phase_hi == 0)
    {
        SERVO_LAT = 1;
        servo_phase_hi = 1;

        tmr0_reload_us(pulse);
    }
    else
    {
        SERVO_LAT = 0;
        servo_phase_hi = 0;

        uint16_t low_us =
            (pulse < 20000u)
            ? (uint16_t) (20000u - pulse)
            : 1000u;

        tmr0_reload_us(low_us);
    }

    T0CON0bits.EN = 1;
}

// =====================================================
// RANDOM
// =====================================================

void prng_seed_from_timers(void)
{
    uint16_t t1 = ((uint16_t) TMR1H << 8) | TMR1L;
    uint16_t t0 = ((uint16_t) TMR0H << 8) | TMR0L;

    uint16_t seed = (uint16_t) (t1 ^ (t0 << 1) ^ 0xBEEF);

    if (seed == 0)
        seed = 0xACE1;

    prng_state ^= seed;

    if (prng_state == 0)
        prng_state = 0xACE1;
}

uint16_t prng_next(void)
{
    uint16_t lsb = prng_state & 1u;

    prng_state >>= 1;

    if (lsb)
        prng_state ^= 0xB400u;

    if (prng_state == 0)
        prng_state = 0xACE1;

    return prng_state;
}

uint16_t rand_range_ms(uint16_t min_ms, uint16_t max_ms)
{
    uint16_t span = (uint16_t) (max_ms - min_ms + 1u);

    return (uint16_t) (min_ms + (prng_next() % span));
}

// =====================================================
// IOC
// =====================================================

void ioc_init_rb1_reset(void)
{
    IVTBASEU = 0x00;
    IVTBASEH = 0x40;
    IVTBASEL = 0x08;

    RESET_TRIS = 1;

    WPUBbits.WPUB1 = 1;

    IOCBFbits.IOCBF1 = 0;
    PIR0bits.IOCIF = 0;

    IOCBNbits.IOCBN1 = 1;
    IOCBPbits.IOCBP1 = 0;

    PIE0bits.IOCIE = 1;
}

void __interrupt(irq(IRQ_IOC), base(0x4008)) IOC_ISR(void)
{
    if (PIR0bits.IOCIF)
    {
        if (IOCBFbits.IOCBF1)
        {
            IOCBFbits.IOCBF1 = 0;

            if (!resetting)
                reset_request = 1;
        }

        PIR0bits.IOCIF = 0;
    }
}

void __interrupt(irq(default), base(0x4008)) DEFAULT_ISR(void)
{
}

// =====================================================
// UI
// =====================================================

void show_ready(void)
{
    char l1[17], l2[17];

    make_line16(l1, "Hit START when");
    make_line16(l2, "ready");

    lcd_goto(1, 0);
    lcd_print_16(l1);

    lcd_goto(2, 0);
    lcd_print_16(l2);
}

void show_started(void)
{
    char l1[17], l2[17];

    make_line16(l1, "Sequence started");
    make_line16(l2, "Get ready!");

    lcd_goto(1, 0);
    lcd_print_16(l1);

    lcd_goto(2, 0);
    lcd_print_16(l2);
}

void false_start_buzz(void)
{
    char l1[17], l2[17];

    make_line16(l1, "Too early!");
    make_line16(l2, "Press RESET");

    lcd_goto(1, 0);
    lcd_print_16(l1);

    lcd_goto(2, 0);
    lcd_print_16(l2);

    buzzer_on();

    for (uint16_t i = 0; i < 500; i++)
    {
        if (reset_request)
            break;

        __delay_ms(1);
    }

    buzzer_off();
}

uint8_t wait_ms_check(uint16_t ms)
{
    while (ms--)
    {
        if (reset_request)
            return 1;

        if (START_PORT == 0)
        {
            false_start_buzz();
            return 2;
        }

        __delay_ms(1);
    }

    return 0;
}

// =====================================================
// F1 SEQUENCE
// =====================================================

uint8_t run_f1_sequence(void)
{
    all_seq_leds_off();

    L1_LAT = 1;
    {
        uint8_t r = wait_ms_check(1000);
        if (r)
            return r;
    }

    L2_LAT = 1;
    {
        uint8_t r = wait_ms_check(1000);
        if (r)
            return r;
    }

    L3_LAT = 1;
    {
        uint8_t r = wait_ms_check(1000);
        if (r)
            return r;
    }

    L4_LAT = 1;
    {
        uint8_t r = wait_ms_check(1000);
        if (r)
            return r;
    }

    L5_LAT = 1;
    {
        uint8_t r = wait_ms_check(1000);
        if (r)
            return r;
    }

    prng_seed_from_timers();

    uint16_t lights_out_delay = rand_range_ms(300, 3000);

    while (lights_out_delay--)
    {
        if (reset_request)
            return 1;

        if (START_PORT == 0)
        {
            false_start_buzz();
            return 2;
        }

        __delay_ms(1);
    }

    all_seq_leds_off();

    return 0;
}

// =====================================================
// REACTION TIMER
// =====================================================

uint32_t measure_reaction_ms(uint16_t timeout_ms)
{
    tmr1_zero();

    uint16_t last = 0;
    uint32_t elapsed_us = 0;

    while (1)
    {
        if (reset_request)
            return 0xFFFFFFFE;

        if (START_PORT == 0)
        {
            __delay_ms(10);

            if (START_PORT == 0)
            {
                uint16_t now =
                    ((uint16_t) TMR1H << 8) | TMR1L;

                if (now >= last)
                    elapsed_us += (uint32_t) (now - last);
                else
                    elapsed_us +=
                        (uint32_t) (65536u - last) + now;

                return (elapsed_us / 1000u);
            }
        }

        uint16_t now =
            ((uint16_t) TMR1H << 8) | TMR1L;

        if (now != last)
        {
            if (now >= last)
                elapsed_us += (uint32_t) (now - last);
            else
                elapsed_us +=
                    (uint32_t) (65536u - last) + now;

            last = now;
        }

        if ((elapsed_us / 1000u) >= timeout_ms)
            return 0xFFFFFFFF;
    }
}

// =====================================================
// REACTION MAP
// =====================================================

uint16_t servo_us_for_reaction_5pt(uint32_t t_ms)
{
    if (t_ms > 500)
        t_ms = 500;

    const uint16_t T[5] = {
        0, 125, 250, 375, 500
    };

    const uint16_t U[5] = {
        2300, 1710, 1180, 700, 390
    };

    uint8_t i = 0;

    while (i < 4 && t_ms > T[i + 1])
        i++;

    uint32_t t0 = T[i];
    uint32_t t1 = T[i + 1];

    uint32_t u0 = U[i];
    uint32_t u1 = U[i + 1];

    uint32_t dt = (t1 - t0);

    uint32_t num =
        (t_ms - t0) *
        ((u0 >= u1) ? (u0 - u1) : (u1 - u0));

    uint32_t u;

    if (u0 >= u1)
        u = u0 - (num / dt);
    else
        u = u0 + (num / dt);

    if (u < SERVO_US_ZERO)
        u = SERVO_US_ZERO;

    if (u > SERVO_US_MAX)
        u = SERVO_US_MAX;

    return (uint16_t) u;
}

// =====================================================
// RESET
// =====================================================

void do_reset_sequence(void)
{
    resetting = 1;
    reset_request = 0;

    READY_LAT = 0;

    buzzer_off();

    all_seq_leds_off();

    servo_pulse_us = SERVO_US_ZERO;

    char l1[17], l2[17];

    make_line16(l1, "Resetting...");
    make_line16(l2, "Please wait");

    lcd_goto(1, 0);
    lcd_print_16(l1);

    lcd_goto(2, 0);
    lcd_print_16(l2);

    for (uint8_t k = 0; k < 15; k++)
    {
        INTLED_LAT ^= 1;
        __delay_ms(100);
    }

    INTLED_LAT = 0;

    while (RESET_PORT == 0);

    resetting = 0;
}

// =====================================================
// MAIN
// =====================================================

typedef enum {
    ST_READY = 0,
    ST_SEQUENCE,
    ST_WAIT_REACTION,
    ST_WAIT_RESET

} state_t;

void main(void)
{
    OSCCON1 = 0x60;
    OSCFRQ = 0x02;

    // RA3 analog enabled
    ANSELA = 0b00001000;
    ANSELB = 0;
    ANSELC = 0;
    ANSELD = 0;

    READY_TRIS = 0;
    READY_LAT = 0;

    INTLED_TRIS = 0;
    INTLED_LAT = 0;

    BUZZ_TRIS = 0;
    BUZZ_LAT = 0;

    L1_TRIS = 0;
    L2_TRIS = 0;
    L3_TRIS = 0;
    L4_TRIS = 0;
    L5_TRIS = 0;

    all_seq_leds_off();

    SERVO_TRIS = 0;
    SERVO_LAT = 0;

    START_TRIS = 1;

    WPUDbits.WPUD2 = 1;

    lcd_init();

    tmr1_init_1us_freerun();

    adc_init();

    servo_pulse_us = SERVO_US_ZERO;

    servo_timer0_init();

    ioc_init_rb1_reset();

    reset_request = 0;

    IOCBFbits.IOCBF1 = 0;
    PIR0bits.IOCIF = 0;

    INTCON0bits.GIE = 1;

    __delay_ms(1500);

    state_t st = ST_READY;

    show_ready();

    READY_LAT = 1;

    while (1)
    {
        if (reset_request)
        {
            do_reset_sequence();

            st = ST_READY;

            show_ready();

            READY_LAT = 1;

            continue;
        }

        if (st == ST_READY)
        {
            if (START_PORT == 0)
            {
                __delay_ms(20);

                if (START_PORT == 0)
                {
                    while (START_PORT == 0)
                    {
                        if (reset_request)
                            break;
                    }

                    prng_seed_from_timers();

                    READY_LAT = 0;

                    show_started();

                    __delay_ms(300);

                    st = ST_SEQUENCE;
                }
            }
        }
        else if (st == ST_SEQUENCE)
        {
            uint8_t r = run_f1_sequence();

            if (r == 1)
                continue;

            if (r == 2)
            {
                st = ST_WAIT_RESET;
                continue;
            }

            st = ST_WAIT_REACTION;
        }
        else if (st == ST_WAIT_REACTION)
        {
            uint32_t t_ms =
                measure_reaction_ms(500);

            if (t_ms == 0xFFFFFFFE)
                continue;

            char l1[17], l2[17];

            if (t_ms == 0xFFFFFFFF)
            {
                make_line16(l1, "Too slow!");
                make_line16(l2, "Press RESET");

                lcd_goto(1, 0);
                lcd_print_16(l1);

                lcd_goto(2, 0);
                lcd_print_16(l2);

                servo_pulse_us = SERVO_US_ZERO;

                st = ST_WAIT_RESET;
            }
            else
            {
                char tmp[32];

                snprintf(tmp, sizeof(tmp),
                         "Reaction:%lums",
                         (unsigned long) t_ms);

                make_line16(l1, tmp);

                servo_pulse_us =
                    servo_us_for_reaction_5pt(t_ms);

                // =====================================
                // ADC FEEDBACK FROM SERVO
                // =====================================

                uint16_t fb = adc_read_an3();

                uint16_t degrees;

                if (fb <= FB_ADC_MIN)
                {
                    degrees = 0;
                }
                else if (fb >= FB_ADC_MAX)
                {
                    degrees = 180;
                }
                else
                {
                    degrees = (uint16_t)
                        (
                            ((uint32_t) (fb - FB_ADC_MIN) * 180u)
                            / (FB_ADC_MAX - FB_ADC_MIN)
                        );
                }

                char pos[17];

                snprintf(pos, sizeof(pos),
                         "Pos:%3u deg",
                         degrees);

                make_line16(l2, pos);

                lcd_goto(1, 0);
                lcd_print_16(l1);

                lcd_goto(2, 0);
                lcd_print_16(l2);

                st = ST_WAIT_RESET;
            }
        }
        else
        {
            __delay_ms(10);
        }
    }
}
