#include "gd32vf103.h"
#include "usb_serial_if.h"
#include "usb_delay.h"
#include <adc.h>
#include <lioonewire.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "systick.h"

#define USE_USB
#define USE_USB_PRINTF

// ─────────────────────────────────────────────
// pH-kalibreringskonstanter
// ─────────────────────────────────────────────
#define PH_OFFSET_MV 1644.0f // mV vid pH 7
#define MV_PER_PH 177.3f     // mV per pH-enhet
#define PH_NEUTRAL 7.0f

void init_timer_settings()
{
    timer_deinit(TIMER5);

    timer_parameter_struct timer_initpara;
    timer_initpara.prescaler = 107;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = 999;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 1;
    timer_init(TIMER5, &timer_initpara);

    timer_enable(TIMER5);
}

void print_ph_value()
{
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    usb_delay_1ms(10); // Ge DMA tid att uppdatera

    float millivolts = (adc_values[1] * 3300.0f) / 4095.0f;
    float ph = PH_NEUTRAL - ((millivolts - PH_OFFSET_MV) / MV_PER_PH);

    if (ph < 0.0f)
        ph = 0.0f;
    if (ph > 14.0f)
        ph = 14.0f;

    int16_t ph_int = (int16_t)ph;
    int16_t ph_dec = (int16_t)((ph - ph_int) * 10.0f);
    if (ph_dec < 0)
        ph_dec = -ph_dec;

    int16_t mv_int = (int16_t)millivolts;

    printf("pH: %d.%d  /  mV: %d\r\n", ph_int, ph_dec, mv_int);
    fflush(0);
}

void print_temp_value()
{
    int16_t raw = lio_read_temp();

    // DS18B20 råvärde är i 1/16-graders steg
    // Division med 16.0f hanterar både positiva och negativa värden korrekt
    float temp = raw / 16.0f;

    // Dela upp i heltal och decimal för printf
    int16_t temp_int = (int16_t)temp;
    int16_t temp_dec = (int16_t)((temp - temp_int) * 100.0f);
    if (temp_dec < 0)
        temp_dec = -temp_dec;

    printf("Temp: %d.%02d C\r\n", temp_int, temp_dec);
    fflush(0);
}

int main()
{
    while (!usb_serial_available())
        usb_delay_1ms(100);
        
    rcu_periph_clock_enable(RCU_TIMER5);
    init_timer_settings();
    ADC3powerUpInit(1);
    lio_init_OW();

    configure_usb_serial();

    while (1)
    {

        print_temp_value();
        usb_delay_1ms(1000);
    }
}