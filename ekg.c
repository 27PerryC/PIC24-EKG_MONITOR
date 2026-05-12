#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "ekg.h"
#include "lcd.h"

#pragma config POSCMOD = NONE
#pragma config I2C1SEL = PRI
#pragma config OSCIOFNC = OFF
#pragma config FCKSM = CSECMD
#pragma config FNOSC = FRCPLL
#pragma config FWDTEN = OFF
#pragma config ICS = PGx2
#pragma config JTAGEN = OFF

static uint16_t threshold     = 600;
static uint16_t refractory_ms = 250;

#define LED_PULSE_MS    50
#define RR_BUFFER_SIZE  4
#define LCD_REFRESH_MS  500
#define NO_SIGNAL_MS    3000

#define LED_PIN         LATBbits.LATB15
#define LED_TRIS        TRISBbits.TRISB15

// state variables //
static volatile uint32_t ms_count = 0;
static volatile uint16_t rr_buffer[RR_BUFFER_SIZE];
static volatile uint8_t  rr_index = 0;
static volatile uint8_t  rr_count = 0;
static volatile uint32_t last_beat_ms = 0;
static volatile uint8_t  has_first_beat = 0;

static uint16_t led_on_timer = 0;
static uint32_t last_lcd_update = 0;

//  UART  //
int __C30_UART = 1;

static void uart1_init(void) {
    __builtin_write_OSCCONL(OSCCON & ~(1<<6));
    RPOR4bits.RP8R = 3;
    RPINR18bits.U1RXR = 9;
    __builtin_write_OSCCONL(OSCCON | (1<<6));
    U1MODE = 0x8000;
    U1STA  = 0x0400;
    U1BRG  = 51;
}

//  ADC  //
static void adc_init(void) {
    AD1CON1 = 0x00E0;
    AD1CON2 = 0x0000;
    AD1CON3 = 0x1F02;
    AD1CHS  = 0;
    AD1PCFG = 0xFFFE;
    AD1CON1bits.ADON = 1;
}

static uint16_t adc_read(void) {
    AD1CON1bits.SAMP = 1;
    for (volatile int i = 0; i < 640; i++);
    AD1CON1bits.SAMP = 0;
    while (!AD1CON1bits.DONE);
    return ADC1BUF0;
}

// Timer1 blocking delay //
static void timer1_init(void) {
    T1CON = 0;
    TMR1 = 0;
    PR1 = 8000;
    T1CONbits.TCKPS = 0;
    T1CONbits.TON = 1;
}

void delay_ms(uint16_t ms) {
    while (ms--) {
        TMR1 = 0;
        while (!IFS0bits.T1IF);
        IFS0bits.T1IF = 0;
    }
}

// Timer2 1 ms tick via interrupt //
static void timer2_init(void) {
    T2CON = 0;
    TMR2 = 0;
    PR2 = 999;
    T2CONbits.TCKPS = 0b01;
    IPC1bits.T2IP = 4;
    IFS0bits.T2IF = 0;
    IEC0bits.T2IE = 1;
    T2CONbits.TON = 1;
}

void __attribute__((__interrupt__, auto_psv)) _T2Interrupt(void) {
    ms_count++;
    IFS0bits.T2IF = 0;
}

static uint32_t get_ms(void) {
    uint32_t v;
    IEC0bits.T2IE = 0;
    v = ms_count;
    IEC0bits.T2IE = 1;
    return v;
}

// LED //
static void led_init(void) {
    LED_TRIS = 0;
    LED_PIN  = 0;
}

// beat detection helpers //
static void record_beat(uint32_t now_ms) {
    if (has_first_beat) {
        uint16_t interval = (uint16_t)(now_ms - last_beat_ms);
        IEC0bits.T2IE = 0;
        rr_buffer[rr_index] = interval;
        rr_index = (rr_index + 1) % RR_BUFFER_SIZE;
        if (rr_count < RR_BUFFER_SIZE) rr_count++;
        IEC0bits.T2IE = 1;
    }
    last_beat_ms = now_ms;
    has_first_beat = 1;
}

static uint16_t compute_bpm(void) {
    if (rr_count < 2) return 0;
    uint32_t now = get_ms();
    if (now - last_beat_ms > NO_SIGNAL_MS) return 0;

    uint8_t n;
    uint32_t sum = 0;
    IEC0bits.T2IE = 0;
    n = rr_count;
    for (uint8_t i = 0; i < n; i++) sum += rr_buffer[i];
    IEC0bits.T2IE = 1;

    uint16_t avg_rr = (uint16_t)(sum / n);
    if (avg_rr == 0) return 0;
    return (uint16_t)(60000UL / avg_rr);
}

// PUBLIC API //

void ekg_init(void) {
    // Initializes all hardware peripherals: pin configuration, UART, ADC,
    // Timer1, Timer2, LCD, and LED
    // Must be called once at startup before any other ekg function
    
    AD1PCFG = 0xFFFE;
    uart1_init();
    adc_init();
    timer1_init();
    timer2_init();
    lcd_init();
    led_init();
}

uint16_t ekg_get_sample(void) {
    // Reads one ADC sample from the EKG sensor, applies the threshold,
    // enforces the refractory period, records a beat if detected, and manages the LED pulse
    // return: filtered ADC value (0 if below threshold or in refractory period)
    
    uint16_t val = adc_read();
    uint16_t filtered = (val > threshold) ? val : 0;

    uint32_t now = get_ms();
    uint32_t ms_since_beat = now - last_beat_ms;

    if (has_first_beat && ms_since_beat < refractory_ms) {
        return 0;
    }

    if (filtered > 0) {
        record_beat(now);
        LED_PIN = 1;
        led_on_timer = LED_PULSE_MS;
    }

    // LED pulse //
    if (led_on_timer > 0) {
        led_on_timer--;
        if (led_on_timer == 0) LED_PIN = 0;
    }

    return filtered;
}

uint16_t ekg_get_bpm(void) {
    // Returns the current BPM averaged over the last 4 R-R intervals (0 if no signal)
    
    return compute_bpm();
}

void ekg_set_threshold(uint16_t t) {
    // Sets the ADC threshold for beat detection (600)
    
    threshold = t;
}

void ekg_set_refractory_ms(uint16_t ms) {
    // Sets the refractory blanking period in ms after each detected beat (250)
    
    refractory_ms = ms;
}

void ekg_stream_serial(void) {
    // Reads a processed EKG sample and sends it over UART as a text number for the serial plotter
    
    uint16_t sample = ekg_get_sample();
    printf("%u\n", sample);
}

void ekg_update_display(void) {
    // Refreshes the LCD with the current BPM every 500 ms
    // Shows "No signal" if no beats detected, or a status message (Low rate / OK / High rate)
    
    uint32_t now = get_ms();
    if (now - last_lcd_update < LCD_REFRESH_MS) return;
    last_lcd_update = now;

    uint16_t bpm = compute_bpm();
    char buf[12];

    if (bpm == 0) {
        lcd_print(0, "BPM: ---");
        lcd_print(1, "No signal");
    } else {
        sprintf(buf, "BPM: %u", bpm);
        lcd_print(0, buf);

        if (bpm < 40)        lcd_print(1, "Low rate");
        else if (bpm > 180)  lcd_print(1, "High rate");
        else                 lcd_print(1, "OK");
    }
}
