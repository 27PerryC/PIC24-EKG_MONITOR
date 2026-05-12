#include "ekg.h"
#include "lcd.h"

int main(void) {
    ekg_init();

    // splash screen //
    lcd_print(0, "EKG Mon");
    lcd_print(1, "Starting..");

    // rough 1-second delay before entering loop //
    delay_ms(1000);

    
    // Infinite loop running every 7ms giving 143 samples per second or 143Hz
    while (1) {
        ekg_stream_serial();
        ekg_update_display();
        delay_ms(7);
    }
}
