//------------------------------
// Title:    Assignment 9 - ADC + LCD + Accelerometer
//------------------------------
// Purpose:  Read the ADXL335 X-axis analog output using the PIC ADC and display
//           the motion state on a 16x2 LCD. Also uses a pushbutton interrupt
//           (IOC) to pause LCD updates and blink a red LED for ~10 seconds.
// Dependencies: XC8 libraries
// Compiler/IDE: MPLAB X IDE v6.30 + XC8 v3.10
// Author:   Carlos Gonzalez
// Date:     4/24/2026
// Inputs:
//   RA0 (AN0)  - Accelerometer X output (analog)
//   RA3 (VREF+) - External ADC reference (3.3V from accelerometer board)
//   RB5        - Pushbutton (active LOW, internal pull-up, IOC interrupt)
// Outputs:
//   RC2        - LCD RS
//   RC3        - LCD E
//   RC4        - LCD D4
//   RC5        - LCD D5
//   RC6        - LCD D6
//   RC7        - LCD D7
//   RE1        - Red LED
// Versions:
//   v1.0 - LCD + ADC voltage display working
//   v2.0 - Accelerometer state detection added (Flat/Tilt/Shake)
//   v3.0 - IOC button pause feature + LCD line clearing fixes
//------------------------------

#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// CONFIG
#pragma config FEXTOSC = OFF
#pragma config RSTOSC  = HFINTOSC_1MHZ
#pragma config CLKOUTEN = OFF
#pragma config CSWEN   = ON
#pragma config FCMEN   = ON
#pragma config MCLRE   = EXTMCLR

#pragma config WDTE    = OFF
#pragma config WDTCCS  = SC

#pragma config BOREN   = ON
#pragma config LPBOREN = OFF
#pragma config STVREN  = ON
#pragma config PPS1WAY = ON
#pragma config LVP     = OFF
#pragma config DEBUG   = OFF
#pragma config CP      = OFF

#define _XTAL_FREQ 1000000UL

// LCD pin mapping
// RS=RC2, E=RC3, D4=RC4, D5=RC5, D6=RC6, D7=RC7
#define LCD_RS_LAT   LATCbits.LATC2
#define LCD_E_LAT    LATCbits.LATC3
#define LCD_D4_LAT   LATCbits.LATC4
#define LCD_D5_LAT   LATCbits.LATC5
#define LCD_D6_LAT   LATCbits.LATC6
#define LCD_D7_LAT   LATCbits.LATC7

#define LCD_RS_TRIS  TRISCbits.TRISC2
#define LCD_E_TRIS   TRISCbits.TRISC3
#define LCD_D4_TRIS  TRISCbits.TRISC4
#define LCD_D5_TRIS  TRISCbits.TRISC5
#define LCD_D6_TRIS  TRISCbits.TRISC6
#define LCD_D7_TRIS  TRISCbits.TRISC7

// Button + Red LED
// Red LED on RE1
#define BTN_TRIS     TRISBbits.TRISB5
#define BTN_PORT     PORTBbits.RB5

#define REDLED_TRIS  TRISEbits.TRISE1
#define REDLED_LAT   LATEbits.LATE1

volatile uint8_t pause_request = 0;
volatile uint8_t paused = 0;   //blocks retrigger while paused

// LCD low-level functions
static void lcd_pulse_enable(void)
{
    LCD_E_LAT = 1;
    __delay_us(2);
    LCD_E_LAT = 0;
    __delay_us(50);
}

static void lcd_write4(uint8_t nibble)
{
    LCD_D4_LAT = (nibble >> 0) & 1;
    LCD_D5_LAT = (nibble >> 1) & 1;
    LCD_D6_LAT = (nibble >> 2) & 1;
    LCD_D7_LAT = (nibble >> 3) & 1;
    lcd_pulse_enable();
}

static void lcd_cmd(uint8_t cmd)
{
    LCD_RS_LAT = 0;
    lcd_write4(cmd >> 4);
    lcd_write4(cmd & 0x0F);
    __delay_ms(2);
}

static void lcd_data(uint8_t data)
{
    LCD_RS_LAT = 1;
    lcd_write4(data >> 4);
    lcd_write4(data & 0x0F);
    __delay_us(60);
}

static void lcd_clear(void)
{
    lcd_cmd(0x01);
    __delay_ms(2);
}

static void lcd_goto(uint8_t row, uint8_t col)
{
    uint8_t addr = (row == 1) ? (0x80 + col) : (0xC0 + col);
    lcd_cmd(addr);
}

static void lcd_print(const char *s)
{
    while (*s) lcd_data((uint8_t)*s++);
}

static void lcd_print_16(const char *s)
{
    // prints EXACTLY 16 characters (pads with spaces)
    for (uint8_t i = 0; i < 16; i++)
    {
        char c = (s && s[i]) ? s[i] : ' ';
        lcd_data((uint8_t)c);
    }
}

// Makes a string exactly 16 chars (pads with spaces)
static void make_line16(char out[17], const char *in)
{
    for (uint8_t i = 0; i < 16; i++) out[i] = ' ';
    out[16] = '\0';

    for (uint8_t i = 0; i < 16 && in[i] != '\0'; i++)
        out[i] = in[i];
}

static void lcd_init(void)
{
    LCD_RS_TRIS = 0; LCD_E_TRIS = 0;
    LCD_D4_TRIS = 0; LCD_D5_TRIS = 0; LCD_D6_TRIS = 0; LCD_D7_TRIS = 0;

    LCD_RS_LAT = 0; LCD_E_LAT = 0;
    LCD_D4_LAT = 0; LCD_D5_LAT = 0; LCD_D6_LAT = 0; LCD_D7_LAT = 0;

    __delay_ms(20);

    LCD_RS_LAT = 0;
    lcd_write4(0x03); __delay_ms(5);
    lcd_write4(0x03); __delay_us(150);
    lcd_write4(0x03); __delay_us(150);
    lcd_write4(0x02); // 4-bit mode

    lcd_cmd(0x28); // 4-bit, 2-line
    lcd_cmd(0x0C); // display ON
    lcd_cmd(0x06); // entry mode
    lcd_clear();
}


// ADC (AN0 = RA0) with VREF+ on RA3
static void adc_init(void)
{
    // RA0 = AN0
    TRISAbits.TRISA0 = 1;
    ANSELAbits.ANSELA0 = 1;

    // RA3 = VREF+
    TRISAbits.TRISA3 = 1;
    ANSELAbits.ANSELA3 = 1;

    // External VREF+ pin, VSS as negative reference
    ADREFbits.PREF = 0b01;
    ADREFbits.NREF = 0;

    ADCON0bits.FM = 1;      // right-justified
    ADCON0bits.CS = 1;      // ADCRC clock
    ADPCH = 0x00;           // AN0
    ADCLK = 0x00;

    ADPREL = 0; ADPREH = 0;
    ADACQL = 0; ADACQH = 0;

    ADCON0bits.ON = 1;
}

static uint16_t adc_read_an0(void)
{
    ADPCH = 0x00;
    __delay_us(20);
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);
    return (((uint16_t)ADRESH << 8) | ADRESL) & 0x0FFF;
}


// IOC setup on RB5 (button)
static void ioc_init_rb5(void)
{
    // Match IVT base to ISR base(0x4008)
    IVTBASEU = 0x00;
    IVTBASEH = 0x40;
    IVTBASEL = 0x08;

    // make PORTB digital
    ANSELB = 0x00;

    BTN_TRIS = 1;                 // RB5 input

    // weak pull-up so button reads HIGH when not pressed
    WPUBbits.WPUB5 = 1;

    // clear old flags
    IOCBFbits.IOCBF5 = 0;
    PIR0bits.IOCIF = 0;

    // falling edge detect (pressed -> LOW)
    IOCBNbits.IOCBN5 = 1;
    IOCBPbits.IOCBP5 = 0;

    PIE0bits.IOCIE = 1;           // enable IOC interrupt
    INTCON0bits.GIE = 1;          // global interrupts ON
}

// IOC ISR
void __interrupt(irq(IRQ_IOC), base(0x4008)) IOC_ISR(void)
{
    if (PIR0bits.IOCIF)
    {
        if (IOCBFbits.IOCBF5)
        {
            IOCBFbits.IOCBF5 = 0; // clear RB5 flag

            // only trigger pause if not already paused
            if (!paused)
                pause_request = 1;
        }

        PIR0bits.IOCIF = 0;       // clear IOC module flag
    }
}


// MAIN: Section 7 + 7.1
void main(void)
{
    // Run HFINTOSC at 4 MHz
    OSCFRQ = 0x02;

    ANSELC = 0x00;
    ANSELE = 0x00;
    ANSELB = 0x00;

    REDLED_TRIS = 0;
    REDLED_LAT = 0;

    lcd_init();
    adc_init();
    ioc_init_rb5();

    // VREF+ = 3.3V (from accelerometer 3Vo)
    const float VREF = 3.3f;

    // Flat trim calibration: keep board FLAT + still during power-up
    float x0_v = 1.65f;
    {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 64; i++)
        {
            sum += adc_read_an0();
            __delay_ms(5);
        }
        float avg = (float)sum / 64.0f;
        x0_v = (avg * VREF) / 4095.0f;
    }

    const float SENS_V_PER_G = 0.300f; 
    const float G_TO_MS2 = 9.80665f;

    uint16_t prev_adc = adc_read_an0();

    while (1)
    {
        // Section 7.1: pause 10s on button press
        if (pause_request)
        {
            pause_request = 0;
            paused = 1;       // lock out retrigger during pause

            for (uint8_t k = 0; k < 100; k++) // 10 seconds total
            {
                REDLED_LAT ^= 1;
                __delay_ms(100);
            }
            REDLED_LAT = 0;

            // wait for button release so it doesn't instantly retrigger
            while (BTN_PORT == 0) { ; }

            // clear any leftover IOC flags
            IOCBFbits.IOCBF5 = 0;
            PIR0bits.IOCIF = 0;

            paused = 0;                 // resume normal operation
        }

        // Section 7: read accelerometer
        uint16_t adc = adc_read_an0();

        float vout = ((float)adc * VREF) / 4095.0f;
        float acc_g = (vout - x0_v) / SENS_V_PER_G;
        float acc_ms2 = acc_g * G_TO_MS2;

        // shake detection (change between samples)
        int32_t d_adc = (int32_t)adc - (int32_t)prev_adc;
        prev_adc = adc;

        float d_v = ((float)d_adc * VREF) / 4095.0f;
        float delta_g = fabsf(d_v / SENS_V_PER_G);

        const char *state;
        if (delta_g > 0.60f || fabsf(acc_g) > 1.20f)
            state = "Shake";
        else if (acc_g > 0.25f)
            state = "Tilt_right";
        else if (acc_g < -0.25f)
            state = "Tilt_Left";
        else
            state = "Flat";

        // Build text first, then force to 16 chars
        char raw1[32];
        char raw2[32];
        char line1_16[17];
        char line2_16[17];

        snprintf(raw1, sizeof(raw1), "State:%s", state);

        if (fabsf(acc_ms2) < 0.050f) acc_ms2 = 0.0f;
        snprintf(raw2, sizeof(raw2), "%0.4f X(m/s^2)", acc_ms2);

        make_line16(line1_16, raw1);
        make_line16(line2_16, raw2);

        lcd_goto(1, 0);
        lcd_print_16(line1_16);

        lcd_goto(2, 0);
        lcd_print_16(line2_16);

        __delay_ms(200);
    }
}
