/*
 * main_mcu.c  –  Algae-Halt sensor firmware
 * Target: GD32VF103 @ 108 MHz
 *
 * Reads:
 *   - pH via ADC (ADC0, channel 1)
 *   - Temperature via DS18B20 on 1-Wire (PB5)
 *
 * Emits over USB-CDC every ~2 s:
 *   SENSOR pH=7.2 temp=23.45\r\n
 *
 * The GUI (main_gui.c) parses that line on COM8.
 */

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

// ── pH calibration ────────────────────────────────────────────────────────────
#define PH_OFFSET_MV 1272.8f // mV at pH 7.0 (calibrate against buffer)
#define MV_PER_PH 170.4341f     // mV per pH unit  (Nernst ~59.16 mV/pH @ 25 °C)
#define PH_NEUTRAL 7.0f
#define CALIBRATION_VALUE_TURBIDITY 4.8f
// ── Timer ─────────────────────────────────────────────────────────────────────
void init_timer_settings(void) {
    timer_deinit(TIMER5);

    timer_parameter_struct p;
    p.prescaler = 107; // 108 MHz / (107+1) = 1 MHz tick
    p.alignedmode = TIMER_COUNTER_EDGE;
    p.counterdirection = TIMER_COUNTER_UP;
    p.period = 999; // overflow every 1 ms
    p.clockdivision = TIMER_CKDIV_DIV1;
    p.repetitioncounter = 1;
    timer_init(TIMER5, &p);
    timer_enable(TIMER5);
}

// ── pH reading ────────────────────────────────────────────────────────────────
// Returns pH clamped to [0.0, 14.0].
float read_ph(void)
{
    adc_software_trigger_enable(ADC0, ADC_REGULAR_CHANNEL);
    usb_delay_1ms(10); // let DMA settle

    float mv = (adc_values[1] * 3300.0f) / 4095.0f;
    float ph = PH_NEUTRAL - ((mv - PH_OFFSET_MV) / MV_PER_PH);

    if (ph < 0.0f)
        ph = 0.0f;
    if (ph > 14.0f)
        ph = 14.0f;
    return ph;
}
float read_NTU(void)
{
    usb_delay_1ms(10);

    float voltage = ((float)adc_values[0] * (5.0f / 4096.0f));
    float ntu;

    if (voltage >= CALIBRATION_VALUE_TURBIDITY) {
        ntu = 0.0f;  // clear water / air → skip formula entirely
    } else {
        ntu = -1120.4f * (voltage * voltage) + 5742.3f * voltage - 4352.9f;
    }

    if (ntu < 0.0f)    ntu = 0.0f;
    if (ntu > 3000.0f) ntu = 3000.0f;

    return ntu;
}
// ── Temperature reading ───────────────────────────────────────────────────────
// Returns temperature in °C, or -999.0 if sensor not found.
float read_temp(void)
{
    uint8_t presence = lio_OW_touch_reset();
    if (presence == 1)
    {
        // No sensor on the bus
        return -999.0f;
    }

    int16_t raw = lio_read_temp();

    // DS18B20 raw value is in 1/16 °C steps
    return raw / 16.0f;
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(void)
{
    rcu_periph_clock_enable(RCU_TIMER5);
    init_timer_settings();
    ADC3powerUpInit(1);
    lio_init_OW(); // initialise 1-Wire on PB5

    configure_usb_serial();

    // Wait for USB host to open the port
    while (!usb_serial_available())
        usb_delay_1ms(100);

    usb_delay_1ms(1000);
    printf("Algae-Halt sensor ready\r\n");
    fflush(0);

    while (1)
    {
        float ph = read_ph();
        float temp = read_temp();
        float ntu = read_NTU();
        float voltage = ((float)adc_values[1]*3300.0f)/4095.0f;
        int adc_valueD = adc_values[1];

        // ── Integer / decimal split (no float printf on this libc) ───────────
        int ph_i = (int)ph;
        int ph_d = (int)((ph - ph_i) * 10.0f);
        if (ph_d < 0)
            ph_d = -ph_d;

        int voltage_i = (int)voltage;
        int voltage_d = (int)((voltage - voltage_i) * 10.0f);
        if (voltage_d < 0)
            voltage_d = -voltage_d;    


        // ── Integer / decimal split (no float printf on this libc) ───────────
        int ntu_i = (int)ntu;
        int ntu_d = (int)((ntu - ntu_i) * 10.0f);
        if (ntu_d < 0)
            ntu_d = -ntu_d;

        if (temp < -100.0f)
        {
            // Sensor missing: send sentinel so the GUI can show "N/A"
            printf("SENSOR pH=%d.%d temp=NONE Voltage=%d.%d ADC: %d \r\n", ph_i, ph_d, voltage_i, voltage_d, adc_valueD);
        }
        else
        {
            int tmp_i = (int)temp;
            int tmp_d = (int)((temp - tmp_i) * 100.0f);
            if (tmp_d < 0)
                tmp_d = -tmp_d;

            // ── Structured output that the GUI parses ────────────────────────
            // Format: SENSOR pH=<int>.<frac> temp=<int>.<frac2>\r\n
            printf("SENSOR pH=%d.%d temp=%d.%02d  Turbidity=%d.%d  Voltage=%d.%d\r\n",
                   ph_i, ph_d, tmp_i, tmp_d, ntu_i, ntu_d, voltage_i, voltage_d);
        }
        fflush(0);

        usb_delay_1ms(2000); // 2 s between readings
    }
}