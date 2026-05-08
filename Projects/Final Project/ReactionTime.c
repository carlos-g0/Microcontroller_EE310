#include <xc.h>
#include <stdint.h>
#include <stdio.h>

// =====================================================
// CONFIG (PIC18F47K42) - WDT off
// =====================================================
#pragma config FEXTOSC  = OFF
#pragma config RSTOSC   = HFINTOSC_1MHZ
#pragma config CLKOUTEN = OFF
#pragma config CSWEN    = ON
#pragma config FCMEN    = ON
#pragma config MCLRE    = EXTMCLR

#pragma config WDTE     = OFF
#pragma config WDTCCS   = SC

#pragma config BOREN    = ON
#pragma config LPBOREN  = OFF
#pragma config STVREN   = ON
#pragma config PPS1WAY  = ON
#pragma config LVP      = OFF
#pragma config DEBUG    = OFF
#pragma config CP       = OFF

// IMPORTANT for multiple irq() ISRs (Timer0 + IOC)
#pragma config MVECEN   = ON

#define _XTAL_FREQ 4000000UL   // run HFINTOSC at 4 MHz

// =====================================================
// PIN MAP (YOUR WIRING)
// =====================================================
// LCD (4-bit): RS=RC2, E=RC3, D4=RC4, D5=RC5, D6=RC6, D7=RD4
#define LCD_RS_LAT   LATCbits.LATC2
#define LCD_E_LAT    LATCbits.LATC3
#define LCD_D4_LAT   LATCbits.LATC4
#define LCD_D5_LAT   LATCbits.LATC5
#define LCD_D6_LAT   LATCbits.LATC6
#define LCD_D7_LAT   LATDbits.LATD4

#define LCD_RS_TRIS  TRISCbits.TRISC2
#define LCD_E_TRIS   TRISCbits.TRISC3
#define LCD_D4_TRIS  TRISCbits.TRISC4
#define LCD_D5_TRIS  TRISCbits.TRISC5
#define LCD_D6_TRIS  TRISCbits.TRISC6
#define LCD_D7_TRIS  TRISDbits.TRISD4

// Start button (active LOW) on RD2
#define START_TRIS   TRISDbits.TRISD2
#define START_PORT   PORTDbits.RD2

// Reset button (active LOW) on RB1 (IOC)
#define RESET_TRIS   TRISBbits.TRISB1
#define RESET_PORT   PORTBbits.RB1

// Buzzer on RD0
#define BUZZ_TRIS    TRISDbits.TRISD0
#define BUZZ_LAT     LATDbits.LATD0

// READY LED on RB3, RESET/INT LED on RB0
#define READY_TRIS   TRISBbits.TRISB3
#define READY_LAT    LATBbits.LATB3

#define INTLED_TRIS  TRISBbits.TRISB0
#define INTLED_LAT   LATBbits.LATB0

// 5-light sequence LEDs (left -> right): RA2, RA1, RB5, RB4, RC7
#define L1_TRIS      TRISAbits.TRISA2
#define L1_LAT       LATAbits.LATA2

#define L2_TRIS      TRISAbits.TRISA1
#define L2_LAT       LATAbits.LATA1

#define L3_TRIS      TRISBbits.TRISB5
#define L3_LAT       LATBbits.LATB5

#define L4_TRIS      TRISBbits.TRISB4
#define L4_LAT       LATBbits.LATB4

#define L5_TRIS      TRISCbits.TRISC7
#define L5_LAT       LATCbits.LATC7

// Servo signal on RA0
#define SERVO_TRIS   TRISAbits.TRISA0
#define SERVO_LAT    LATAbits.LATA0

// =====================================================
// SERVO PULSE RANGE
// =====================================================
#define SERVO_US_ZERO  390     // slow end / "zero" 
#define SERVO_US_MAX   2300    // fast end / max

// =====================================================
// GLOBALS
// =====================================================
volatile uint8_t  reset_request = 0;
volatile uint8_t  resetting     = 0;

// Timer0 servo driver variables
volatile uint16_t servo_pulse_us = SERVO_US_ZERO;  // 390..2300
volatile uint8_t  servo_phase_hi = 0;              // 0->next HIGH, 1->next LOW

// PRNG
static uint16_t prng_state = 0xACE1;

// =====================================================
// HELPERS
// =====================================================
static void all_seq_leds_off(void){ L1_LAT=0; L2_LAT=0; L3_LAT=0; L4_LAT=0; L5_LAT=0; }
static void buzzer_off(void){ BUZZ_LAT=0; }
static void buzzer_on(void){ BUZZ_LAT=1; }

static void make_line16(char out[17], const char *in)
{
    for (uint8_t i=0;i<16;i++) out[i]=' ';
    out[16]='\0';
    for (uint8_t i=0;i<16 && in[i]!='\0'; i++) out[i]=in[i];
}

// =====================================================
// LCD 4-bit
// =====================================================
static void lcd_pulse_enable(void)
{
    LCD_E_LAT=1; __delay_us(2);
    LCD_E_LAT=0; __delay_us(50);
}
static void lcd_write4(uint8_t n)
{
    LCD_D4_LAT=(n>>0)&1;
    LCD_D5_LAT=(n>>1)&1;
    LCD_D6_LAT=(n>>2)&1;
    LCD_D7_LAT=(n>>3)&1;
    lcd_pulse_enable();
}
static void lcd_cmd(uint8_t c)
{
    LCD_RS_LAT=0;
    lcd_write4(c>>4);
    lcd_write4(c&0x0F);
    __delay_ms(2);
}
static void lcd_data(uint8_t d)
{
    LCD_RS_LAT=1;
    lcd_write4(d>>4);
    lcd_write4(d&0x0F);
    __delay_us(60);
}
static void lcd_goto(uint8_t row,uint8_t col)
{
    uint8_t addr=(row==1)?(0x80+col):(0xC0+col);
    lcd_cmd(addr);
}
static void lcd_print_16(const char *s16)
{
    for(uint8_t i=0;i<16;i++) lcd_data((uint8_t)s16[i]);
}
static void lcd_init(void)
{
    LCD_RS_TRIS=0; LCD_E_TRIS=0;
    LCD_D4_TRIS=0; LCD_D5_TRIS=0; LCD_D6_TRIS=0; LCD_D7_TRIS=0;

    LCD_RS_LAT=0; LCD_E_LAT=0;
    LCD_D4_LAT=0; LCD_D5_LAT=0; LCD_D6_LAT=0; LCD_D7_LAT=0;

    __delay_ms(20);

    lcd_write4(0x03); __delay_ms(5);
    lcd_write4(0x03); __delay_us(150);
    lcd_write4(0x03); __delay_us(150);
    lcd_write4(0x02);

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);
    __delay_ms(2);
}

// =====================================================
// TIMER1 free-run (reaction measurement + entropy)
// FOSC=4MHz => FOSC/4 = 1MHz => 1 tick = 1 us
// =====================================================
static void tmr1_init_1us_freerun(void)
{
    T1CONbits.ON = 0;
    T1CLK = 0x01;              // FOSC/4
    T1CONbits.CKPS = 0b00;     // 1:1
    TMR1H = 0;
    TMR1L = 0;
    T1CONbits.ON = 1;
}
static void tmr1_zero(void){ TMR1H=0; TMR1L=0; }

// =====================================================
// TIMER0 SERVO DRIVER (16-bit, 1us ticks)
// ISR alternates HIGH for pulse_us and LOW for (20000 - pulse_us)
// =====================================================
static void tmr0_reload_us(uint16_t us)
{
    uint16_t preload = (uint16_t)(65536u - us);
    TMR0H = (uint8_t)(preload >> 8);
    TMR0L = (uint8_t)(preload & 0xFF);
}

static void servo_timer0_init(void)
{
    // Timer0: 16-bit, FOSC/4, prescaler 1:1
    T0CON0bits.EN = 0;
    T0CON0bits.MD16 = 1;      // 16-bit
    T0CON0bits.OUTPS = 0;     // 1:1 postscaler

    T0CON1bits.CS = 0b010;    // FOSC/4
    T0CON1bits.CKPS = 0b0000; // prescaler 1:1
    T0CON1bits.ASYNC = 0;     // synchronized

    SERVO_LAT = 0;
    servo_phase_hi = 0;

    // first interval before first rising edge
    tmr0_reload_us(1000);

    PIR3bits.TMR0IF = 0;
    PIE3bits.TMR0IE = 1;

    T0CON0bits.EN = 1;
}

void __interrupt(irq(IRQ_TMR0), base(0x4008)) TMR0_ISR(void)
{
    PIR3bits.TMR0IF = 0;

    uint16_t pulse = servo_pulse_us;
    if (pulse < SERVO_US_ZERO) pulse = SERVO_US_ZERO;
    if (pulse > SERVO_US_MAX)  pulse = SERVO_US_MAX;

    // Stop timer while reloading (more reliable on K42)
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
        uint16_t low_us = (pulse < 20000u) ? (uint16_t)(20000u - pulse) : 1000u;
        tmr0_reload_us(low_us);
    }

    T0CON0bits.EN = 1;
}

// =====================================================
// PRNG for random lights-out delay (seed from timers)
// =====================================================
static void prng_seed_from_timers(void)
{
    uint16_t t1 = ((uint16_t)TMR1H << 8) | TMR1L;
    uint16_t t0 = ((uint16_t)TMR0H << 8) | TMR0L;
    uint16_t seed = (uint16_t)(t1 ^ (t0 << 1) ^ 0xBEEF);

    if (seed == 0) seed = 0xACE1;
    prng_state ^= seed;
    if (prng_state == 0) prng_state = 0xACE1;
}
static uint16_t prng_next(void)
{
    uint16_t lsb = prng_state & 1u;
    prng_state >>= 1;
    if (lsb) prng_state ^= 0xB400u;
    if (prng_state == 0) prng_state = 0xACE1;
    return prng_state;
}
static uint16_t rand_range_ms(uint16_t min_ms, uint16_t max_ms)
{
    uint16_t span = (uint16_t)(max_ms - min_ms + 1u);
    return (uint16_t)(min_ms + (prng_next() % span));
}

// =====================================================
// IOC on RB1 (RESET button)
// =====================================================
static void ioc_init_rb1_reset(void)
{
    IVTBASEU = 0x00;
    IVTBASEH = 0x40;
    IVTBASEL = 0x08;

    ANSELB = 0x00;

    RESET_TRIS = 1;
    WPUBbits.WPUB1 = 1;

    IOCBFbits.IOCBF1 = 0;
    PIR0bits.IOCIF = 0;

    IOCBNbits.IOCBN1 = 1;   // falling edge
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
            if (!resetting) reset_request = 1;
        }
        PIR0bits.IOCIF = 0;
    }
}

// Default ISR so accidental flags don’t reset the chip
void __interrupt(irq(default), base(0x4008)) DEFAULT_ISR(void)
{
    // leave empty
}

// =====================================================
// UI screens
// =====================================================
static void show_ready(void)
{
    char l1[17], l2[17];
    make_line16(l1, "Hit START when");
    make_line16(l2, "ready");
    lcd_goto(1,0); lcd_print_16(l1);
    lcd_goto(2,0); lcd_print_16(l2);
}
static void show_started(void)
{
    char l1[17], l2[17];
    make_line16(l1, "Sequence started");
    make_line16(l2, "Get ready!");
    lcd_goto(1,0); lcd_print_16(l1);
    lcd_goto(2,0); lcd_print_16(l2);
}

static void false_start_buzz(void)
{
    char l1[17], l2[17];
    make_line16(l1, "Too early!");
    make_line16(l2, "Press RESET");
    lcd_goto(1,0); lcd_print_16(l1);
    lcd_goto(2,0); lcd_print_16(l2);

    buzzer_on();
    for (uint16_t i=0;i<500;i++)
    {
        if (reset_request) break;
        __delay_ms(1);
    }
    buzzer_off();
}

// wait ms while checking reset + false start
// return: 0 ok, 1 reset, 2 false start
static uint8_t wait_ms_check(uint16_t ms)
{
    while (ms--)
    {
        if (reset_request) return 1;

        if (START_PORT == 0)
        {
            false_start_buzz();
            return 2;
        }

        __delay_ms(1);
    }
    return 0;
}

// F1 sequence
static uint8_t run_f1_sequence(void)
{
    all_seq_leds_off();

    L1_LAT=1; { uint8_t r=wait_ms_check(1000); if(r) return r; }
    L2_LAT=1; { uint8_t r=wait_ms_check(1000); if(r) return r; }
    L3_LAT=1; { uint8_t r=wait_ms_check(1000); if(r) return r; }
    L4_LAT=1; { uint8_t r=wait_ms_check(1000); if(r) return r; }
    L5_LAT=1; { uint8_t r=wait_ms_check(1000); if(r) return r; }

    prng_seed_from_timers();
    uint16_t lights_out_delay = rand_range_ms(300, 3000);
    { uint8_t r=wait_ms_check(lights_out_delay); if(r) return r; }

    all_seq_leds_off();
    return 0;
}

// reaction time (0..timeout_ms)
static uint32_t measure_reaction_ms(uint16_t timeout_ms)
{
    tmr1_zero();

    uint16_t last = 0;
    uint32_t elapsed_us = 0;

    while (1)
    {
        if (reset_request) return 0xFFFFFFFE;

        if (START_PORT == 0)
        {
            __delay_ms(10);
            if (START_PORT == 0)
            {
                uint16_t now = ((uint16_t)TMR1H<<8) | TMR1L;
                if (now >= last) elapsed_us += (uint32_t)(now - last);
                else             elapsed_us += (uint32_t)(65536u - last) + now;
                return (elapsed_us / 1000u);
            }
        }

        uint16_t now = ((uint16_t)TMR1H<<8) | TMR1L;
        if (now != last)
        {
            if (now >= last) elapsed_us += (uint32_t)(now - last);
            else             elapsed_us += (uint32_t)(65536u - last) + now;
            last = now;
        }

        if ((elapsed_us / 1000u) >= timeout_ms)
            return 0xFFFFFFFF;
    }
}

// 5-point calibrated piecewise-linear mapping
static uint16_t servo_us_for_reaction_5pt(uint32_t t_ms)
{
    if (t_ms > 500) t_ms = 500;

    // ms -> us (your calibration)
    const uint16_t T[5] = { 0,   125, 250, 375, 500 };
    const uint16_t U[5] = { 2300,1710,1180,700, 390 };

    uint8_t i = 0;
    while (i < 4 && t_ms > T[i+1]) i++;

    uint32_t t0 = T[i], t1 = T[i+1];
    uint32_t u0 = U[i], u1 = U[i+1];

    uint32_t dt = (t1 - t0);
    uint32_t num = (t_ms - t0) * ( (u0 >= u1) ? (u0 - u1) : (u1 - u0) );

    uint32_t u;
    if (u0 >= u1) u = u0 - (num / dt);
    else          u = u0 + (num / dt);

    if (u < SERVO_US_ZERO) u = SERVO_US_ZERO;
    if (u > SERVO_US_MAX)  u = SERVO_US_MAX;

    return (uint16_t)u;
}

// Reset: blink RB0 for 1.5s, command servo to ZERO
static void do_reset_sequence(void)
{
    resetting = 1;
    reset_request = 0;

    READY_LAT = 0;
    buzzer_off();
    all_seq_leds_off();

    prng_seed_from_timers();

    servo_pulse_us = SERVO_US_ZERO;   // command to zero

    char l1[17], l2[17];
    make_line16(l1, "Resetting...");
    make_line16(l2, "Please wait");
    lcd_goto(1,0); lcd_print_16(l1);
    lcd_goto(2,0); lcd_print_16(l2);

    // 1.5 seconds blink
    for (uint8_t k=0; k<15; k++)
    {
        INTLED_LAT ^= 1;
        __delay_ms(100);
    }
    INTLED_LAT = 0;

    while (RESET_PORT == 0) { ; }

    resetting = 0;
}

// =====================================================
// MAIN
// =====================================================
typedef enum { ST_READY=0, ST_SEQUENCE, ST_WAIT_REACTION, ST_WAIT_RESET } state_t;

void main(void)
{
    // 4 MHz HFINTOSC
    OSCCON1 = 0x60;
    OSCFRQ  = 0x02;

    // all digital
    ANSELA=0; ANSELB=0; ANSELC=0; ANSELD=0;

    // outputs
    READY_TRIS=0; READY_LAT=0;
    INTLED_TRIS=0; INTLED_LAT=0;
    BUZZ_TRIS=0; BUZZ_LAT=0;

    L1_TRIS=0; L2_TRIS=0; L3_TRIS=0; L4_TRIS=0; L5_TRIS=0;
    all_seq_leds_off();

    // servo
    SERVO_TRIS=0; SERVO_LAT=0;

    // inputs + pullups
    START_TRIS=1;
    WPUDbits.WPUD2 = 1;

    // init LCD + timers
    lcd_init();
    tmr1_init_1us_freerun();

    // init IOC + servo timer
    servo_pulse_us = SERVO_US_ZERO;
    servo_timer0_init();
    ioc_init_rb1_reset();

    // clear any startup IOC flags
    reset_request = 0;
    IOCBFbits.IOCBF1 = 0;
    PIR0bits.IOCIF = 0;

    // global interrupts ON
    INTCON0bits.GIE = 1;

    // let servo reach zero
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
                    while (START_PORT == 0) { if(reset_request) break; }

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

            if (r == 1) continue;
            if (r == 2) { st = ST_WAIT_RESET; continue; }

            st = ST_WAIT_REACTION;
        }
        else if (st == ST_WAIT_REACTION)
        {
            uint32_t t_ms = measure_reaction_ms(500);
            if (t_ms == 0xFFFFFFFE) continue;

            char l1[17], l2[17];

            if (t_ms == 0xFFFFFFFF)
            {
                make_line16(l1, "Too slow!");
                make_line16(l2, "Press RESET");
                lcd_goto(1,0); lcd_print_16(l1);
                lcd_goto(2,0); lcd_print_16(l2);

                servo_pulse_us = SERVO_US_ZERO;  // too slow -> zero end
                st = ST_WAIT_RESET;
            }
            else
            {
                char tmp[32];
                snprintf(tmp, sizeof(tmp), "Reaction:%lums", (unsigned long)t_ms);
                make_line16(l1, tmp);
                make_line16(l2, "Press RESET");
                lcd_goto(1,0); lcd_print_16(l1);
                lcd_goto(2,0); lcd_print_16(l2);

                servo_pulse_us = servo_us_for_reaction_5pt(t_ms);
                st = ST_WAIT_RESET;
            }
        }
        else
        {
            __delay_ms(10);
        }
    }
}
