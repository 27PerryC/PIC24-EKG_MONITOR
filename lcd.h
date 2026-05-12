#ifndef LCD_H
#define LCD_H

void lcd_init(void);
void lcd_clear(void);
void lcd_set_cursor(char x, char y);
void lcd_print(char row, const char *str);
void lcd_print_int(char row, int val);

#endif
