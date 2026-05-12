#ifndef EKG_H
#define EKG_H

#include <stdint.h>

void     ekg_init(void);
void     delay_ms(uint16_t ms);
uint16_t ekg_get_sample(void);
uint16_t ekg_get_bpm(void);
void     ekg_set_threshold(uint16_t t);
void     ekg_set_refractory_ms(uint16_t ms);
void     ekg_stream_serial(void);
void     ekg_update_display(void);

#endif
