/*
 * File:   onewire.c
 * Author: remahl
 *
 * Created on September 19, 2019, 11:40 AM
 */

#include <stdint.h>
#include "lioonewire.h"
#include "gd32vf103.h"

#define SKIP_ROM            0xCC
#define WRITE_SCRATCH_PAD   0x4E
#define READ_SCRATCH_PAD    0xBE
#define START_CONVERSION    0x44
#define COPY_SCRATCHPAD     0x48
#define RECALL_EEPROM       0xB8
#define READ_POWER_SUPPLY   0xB4

#define CONFIG_9BIT         0x1F
#define CONFIG_10BIT        0x3F
#define CONFIG_11BIT        0x5F
#define CONFIG_12BIT        0x7F

#define T_A 6
#define T_B 64
#define T_C 60
#define T_D 10
#define T_E 9
#define T_F 55
#define T_G 0
#define T_H 480
#define T_I 70
#define T_J 410

#define DPIN  GPIO_PIN_5   
#define DPORT GPIOB




void dpin_drive(){
    gpio_bit_reset(DPORT, DPIN);
}
void dpin_release(){
    gpio_bit_set(DPORT, DPIN);
}

uint8_t dpin_sample(){
    return gpio_input_bit_get(DPORT, DPIN);
}


int first_read = 1;


void lio_init_OW(){
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(DPORT, GPIO_MODE_OUT_OD, GPIO_OSPEED_50MHZ, DPIN);
}


void lio_OW_write_bit(uint8_t val){
    if(val){
        dpin_drive();
        tick_delay(T_A);
        dpin_release();
        tick_delay(T_B);
    }
    else{
        dpin_drive();
        tick_delay(T_C);
        dpin_release();
        tick_delay(T_D);
    }
}

uint8_t lio_OW_read_bit(){
    uint8_t res = 0;
    dpin_drive();
    tick_delay(T_A);
    dpin_release();
    tick_delay(T_E);
    res = dpin_sample();
    tick_delay(T_F);
    return res;
}

uint8_t lio_OW_read_byte()
{
    uint8_t result=0;

    for (uint8_t i = 0; i < 8; i++){
            result >>= 1;

            if (lio_OW_read_bit())result |= 0x80;
    }
    return result;
}

void lio_OW_write_byte(uint8_t data){
    for(int i = 0; i < 8; i++){
        lio_OW_write_bit(data & 0x01);
        data >>= 1;
    }
}

uint8_t lio_OW_touch_reset(void)
{
    uint8_t res = 0;
    tick_delay(T_G);
    dpin_drive();
    tick_delay(T_H);
    dpin_release();
    tick_delay(T_I);
    res = dpin_sample();
    tick_delay(T_J);
    return res;
}

int16_t lio_read_temp(){ // Användbar.
    
    uint8_t temp_L, temp_H = 0;
    
    if(first_read){
        lio_OW_touch_reset();
        lio_OW_write_byte(SKIP_ROM);
        lio_OW_write_byte(WRITE_SCRATCH_PAD);
        lio_OW_write_byte(0xFF);
        lio_OW_write_byte(0xFF);
        lio_OW_write_byte(CONFIG_12BIT);
        first_read = 0;
    }
              
    lio_OW_touch_reset();
    lio_OW_write_byte(SKIP_ROM);
    lio_OW_write_byte(START_CONVERSION);
    for(int i = 1000; i > 0 && lio_OW_read_byte(); i--);
    lio_OW_touch_reset();
    lio_OW_write_byte(SKIP_ROM);
    lio_OW_write_byte(READ_SCRATCH_PAD);
    temp_L = lio_OW_read_byte();
    temp_H = lio_OW_read_byte();
    return (int16_t) (((temp_H << 8) & 0xFF00) | temp_L);
}
