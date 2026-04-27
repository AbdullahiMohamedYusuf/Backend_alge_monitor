
#pragma once
/* This library is built using the directions provided in Maxim's 126 application note
   https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html 
   it contains lots of good information on the protocol. */



#include <stdint.h>
#include "systick.h"
#include "gd32vf103.h"
/* Timing values for 1-wire protocol in microseconds, check the above app note for more info*/



#define tick_delay(t)  delay_1us(t)    
void lio_init_OW();
void lio_OW_write_bit(uint8_t val);
uint8_t lio_OW_read_bit(void);
uint8_t lio_OW_read_byte(void);
uint8_t lio_OW_touch_reset(void);
int16_t lio_read_temp(void);
//void DS18B20_get_string(char* p_str, int16_t temp);
void lio_OW_write_byte(uint8_t data);


