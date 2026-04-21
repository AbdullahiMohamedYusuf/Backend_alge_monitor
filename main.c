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

// ─────────────────────────────────────────────
// pH-kalibreringskonstanter
// Steg 1: Doppa i pH 7-lösning, läs av mV → sätt PH_OFFSET_MV
// Steg 2: Doppa i pH 4-lösning, läs av mV → MV_PER_PH = (PH_OFFSET_MV - ph4_mv) / 3.0
// ─────────────────────────────────────────────
#define PH_OFFSET_MV   1644.0f   // mV vid pH 7 (justera efter kalibrering!)
#define MV_PER_PH      177.3f    // mV per pH-enhet (justera efter kalibrering!)
#define PH_NEUTRAL     7.0f

void init_timer_settings()
{
    timer_deinit(TIMER5);

    timer_parameter_struct timer_initpara;
    timer_initpara.prescaler         = 107;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 999;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 1;
    timer_init(TIMER5, &timer_initpara);

    timer_enable(TIMER5);
}

int main()
{
    rcu_periph_clock_enable(RCU_TIMER5);
    init_timer_settings();
    ADC3powerUpInit(1);
    lio_init_OW();

    configure_usb_serial();

    while (!usb_serial_available())
        usb_delay_1ms(100);

    usb_delay_1ms(1000);
    printf("main is alive\r\n");
    fflush(0);

    usb_delay_1ms(1000);
    printf("Entering loop\r\n");
    fflush(0);

    while (1)
    {
        adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
        usb_delay_1ms(10); // Ge DMA tid att uppdatera

        // ── Råvärde → millivolt (float) ──────────────────────────
        // adc_values[0] = PA3 = CHANNEL_3
        // adc_values[1] = PA1 = CHANNEL_1  ← pH-kortet kopplat hit
        float millivolts = (adc_values[1] * 3300.0f) / 4095.0f;

        // ── pH-beräkning med flyttal ──────────────────────────────
        // Formel: pH = 7 - (mV - offset) / känslighet
        float ph = PH_NEUTRAL - ((millivolts - PH_OFFSET_MV) / MV_PER_PH);

        // ── Begränsa till rimligt pH-område ──────────────────────
        if (ph < 0.0f)  ph = 0.0f;
        if (ph > 14.0f) ph = 14.0f;

        // ── Dela upp i heltal + decimal för printf ────────────────
        // (GD32 printf stödjer inte alltid %f direkt)
        int16_t ph_int = (int16_t)ph;
        int16_t ph_dec = (int16_t)((ph - ph_int) * 10.0f);
        if (ph_dec < 0) ph_dec = -ph_dec;

        int16_t mv_int = (int16_t)millivolts;

        printf("pH: %d.%d  /  mV: %d\r\n", ph_int, ph_dec, mv_int);
        fflush(0);
        usb_delay_1ms(500);
    }
}