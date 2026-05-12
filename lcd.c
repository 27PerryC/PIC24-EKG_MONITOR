#include <xc.h>
#include <stdio.h>
#include "lcd.h"

#define LCD_ADDR        0b0111100
#define LCD_ADDR_WRITE  ((LCD_ADDR << 1) & 0xFE)   // 0x78
#define LCD_CONTROL_CMD  0x00
#define LCD_CONTROL_DATA 0x40
#define LCD_ROW0_ADDR    0x00
#define LCD_ROW1_ADDR    0x20
 
// delay (runs before timers are up) //
static void delay_ms_lcd(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 1600; j++) {
            asm("nop");
        }
    }
}

// initialize the LCD //
static void i2c_init(void) {
    I2C2CONbits.I2CEN = 0;
    I2C2BRG = 78;              // 100 kHz @ Fcy = 8 MHz
    _MI2C2IF = 0;
    I2C2CONbits.I2CEN = 1;
}

static void i2c_start(void) {
    I2C2CONbits.SEN = 1;
    while (I2C2CONbits.SEN);
}

static void i2c_stop(void) {
    I2C2CONbits.PEN = 1;
    while (I2C2CONbits.PEN);
}

static void i2c_write(char data) {
    _MI2C2IF = 0;
    I2C2TRN = data;
    while (!_MI2C2IF || I2C2STATbits.TRSTAT);
}

// low-level LCD commands //
static void lcd_cmd(char command) {
    i2c_start();
    i2c_write(LCD_ADDR_WRITE);
    i2c_write(LCD_CONTROL_CMD);
    i2c_write(command);
    i2c_stop();
}

static void lcd_printChar(char myChar) {
    i2c_start();
    i2c_write(LCD_ADDR_WRITE);
    i2c_write(LCD_CONTROL_DATA);
    i2c_write(myChar);
    i2c_stop();
}

// public functions //

void lcd_init(void) {
    // Initializes the LCD display over I2C
    // Sets up I2C2 bus, hardware resets the display using RB6,
    // sends the SSD1803A initialization sequence,
    // configures 2 line double height mode, and clears the screen
    // No parameters
    
    i2c_init();

    TRISBbits.TRISB6 = 0;
    LATBbits.LATB6 = 0;
    delay_ms_lcd(50);
    LATBbits.LATB6 = 1;
    delay_ms_lcd(50);

    // SSD1803A initialization sequence
    lcd_cmd(0x3A);  delay_ms_lcd(2);
    lcd_cmd(0x09);  delay_ms_lcd(2);
    lcd_cmd(0x06);  delay_ms_lcd(2);
    lcd_cmd(0x1E);  delay_ms_lcd(2);
    lcd_cmd(0x39);  delay_ms_lcd(2);
    lcd_cmd(0x1B);  delay_ms_lcd(2);
    lcd_cmd(0x6E);  delay_ms_lcd(200);
    lcd_cmd(0x56);  delay_ms_lcd(2);
    lcd_cmd(0x7A);  delay_ms_lcd(2);
    lcd_cmd(0x38);  delay_ms_lcd(2);
    lcd_cmd(0x0F);  delay_ms_lcd(2);

    // 2-line double-height mode
    lcd_cmd(0x3A);  delay_ms_lcd(2);
    lcd_cmd(0x09);  delay_ms_lcd(2);
    lcd_cmd(0x1A);  delay_ms_lcd(2);
    lcd_cmd(0x3C);  delay_ms_lcd(2);

    lcd_cmd(0x01);  delay_ms_lcd(2);
    lcd_cmd(0x06);  delay_ms_lcd(2);
}

void lcd_clear(void) {
    // Clears all characters from the LCD and resets the cursor to position 0
    // No parameters
    
    lcd_cmd(0x01);
    delay_ms_lcd(2);
}

void lcd_set_cursor(char x, char y) {
    // Moves the LCD cursor to a specific position
    // x: column position (0-9)
    // y: row number (0 = top, 1 = bottom)  
    
    char addr = (y == 0) ? (LCD_ROW0_ADDR + x) : (LCD_ROW1_ADDR + x);
    lcd_cmd(0x80 | addr);
}

void lcd_print(char row, const char *str) {
    // Writes a string to one row of the LCD, padding with spaces to fill all 10 columns
    // row: row number (0 = top, 1 = bottom)
    // str: null-terminated string to display
    
    char i;
    lcd_set_cursor(0, row);
    for (i = 0; i < 10; i++) {
        if (*str != '\0') {
            lcd_printChar(*str);
            str++;
        } else {
            lcd_printChar(' ');
        }
    }
}

void lcd_print_int(char row, int val) {
    // Converts an integer to a string and displays it on the given LCD row
    // row: row number (0 = top, 1 = bottom)
    // val: integer value to display
    
    char buf[12];
    sprintf(buf, "%d", val);
    lcd_print(row, buf);
}
