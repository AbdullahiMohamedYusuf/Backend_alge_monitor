////////////// VERSION 7 ////////////////////////


#include "gd32vf103.h"
#include "usb_serial_if.h"
#include "usb_delay.h"
#include <adc.h>
#include <lioonewire.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define USE_USB
#define USE_USB_PRINTF

#define PH_OFFSET_MV 1272.8f    
#define MV_PER_PH    170.4341f 
#define PH_NEUTRAL   7.0f


#define NTU_V_PEAK         2.56f
#define NTU_V_CLEAR        4.20f
#define NTU_MAX            3000.0f
#define NTU_EXTRAP_SLOPE   1500.0f   

void init_timer_settings(void) {
    timer_deinit(TIMER5);

    timer_parameter_struct p;
    p.prescaler         = 107;                  
    p.alignedmode       = TIMER_COUNTER_EDGE;
    p.counterdirection  = TIMER_COUNTER_UP;
    p.period            = 999;                  
    p.clockdivision     = TIMER_CKDIV_DIV1;
    p.repetitioncounter = 1;
    timer_init(TIMER5, &p);
    timer_enable(TIMER5);
}

float read_ph(void)
{
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    usb_delay_1ms(10); 

    float mv = (adc_values[1] * 3300.0f) / 4095.0f;
    float ph = PH_NEUTRAL - ((mv - PH_OFFSET_MV) / MV_PER_PH);

    if (ph < 0.0f)  ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

float read_NTU(void)
{
    usb_delay_1ms(10);

    // 12-bit ADC, 0..4095, ref 3.3 V
    float voltage = ((float)adc_values[0] * 3.3f) / 4095.0f;
    float v = voltage * (4.5f / 3.3f);

    float ntu;

    if (v >= NTU_V_CLEAR) {
        ntu = 0.0f;
    }
    else if (v >= NTU_V_PEAK) {
        ntu = -1120.4f * v * v + 5742.3f * v - 4352.9f;
        if (ntu < 0.0f)    ntu = 0.0f;
        if (ntu > NTU_MAX) ntu = NTU_MAX;
    }
    else {
        
        ntu = NTU_MAX + (NTU_V_PEAK - v) * (NTU_EXTRAP_SLOPE / NTU_V_PEAK);
        if (ntu > NTU_MAX + NTU_EXTRAP_SLOPE) {
            ntu = NTU_MAX + NTU_EXTRAP_SLOPE;
        }
    }

    return ntu;
}

float read_temp(void) // Temp sensor funktion
{
    uint8_t presence = lio_OW_touch_reset(); // Linus funktion
    if (presence == 1)
    {
        return -999.0f;
    }

    int16_t raw = lio_read_temp();

    return raw / 16.0f;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void)
{
    rcu_periph_clock_enable(RCU_TIMER5);
    init_timer_settings();
    ADC3powerUpInit(1);
    lio_init_OW();        // startar OneWire

    configure_usb_serial();

    while (!usb_serial_available())
        usb_delay_1ms(100);

    usb_delay_1ms(1000);
    printf("Algae-Halt sensor ready\r\n");
    fflush(0);

    while (1)
    {
        printf("RAW[0]=%d RAW[1]=%d\r\n", adc_values[0], adc_values[1]);

        float ph      = read_ph();
        float temp    = read_temp();
        float ntu     = read_NTU();
        float voltage = ((float)adc_values[0] * 3300.0f) / 4095.0f; // convertering till spänning
        int   adc_valueD = adc_values[0];

        int ph_i = (int)ph;
        int ph_d = (int)((ph - ph_i) * 10.0f);
        if (ph_d < 0) ph_d = -ph_d;

        int voltage_i = (int)voltage;
        int voltage_d = (int)((voltage - voltage_i) * 10.0f);
        if (voltage_d < 0) voltage_d = -voltage_d;

        int ntu_i = (int)ntu;
        int ntu_d = (int)((ntu - ntu_i) * 10.0f);
        if (ntu_d < 0) ntu_d = -ntu_d;

        if (temp < -100.0f)
        { // Om sensorn returerar inget
            printf("SENSOR pH=%d.%d temp=NONE Turbidity=%d.%d Voltage=%d.%d ADC: %d \r\n",
                   ph_i, ph_d, ntu_i, ntu_d, voltage_i, voltage_d, adc_valueD);
        }
        else
        {
            int tmp_i = (int)temp;
            int tmp_d = (int)((temp - tmp_i) * 100.0f);
            if (tmp_d < 0) tmp_d = -tmp_d;

           
            printf("SENSOR pH=%d.%d temp=%d.%02d Turbidity=%d.%d Voltage=%d.%d\r\n",
                   ph_i, ph_d, tmp_i, tmp_d, ntu_i, ntu_d, voltage_i, voltage_d);
        }
        fflush(0);

        usb_delay_1ms(2000); 
    }
}