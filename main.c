////////////// VERSION 7 ////////////////////////
/*
 * main_mcu.c  –  Algae-Halt sensor firmware
 * Target: GD32VF103 @ 108 MHz
 *
 * Reads:
 *   - pH via ADC (ADC0, channel 1)
 *   - Turbidity (NTU) via ADC (ADC0, channel 0) – DFRobot SEN0189
 *   - Temperature via DS18B20 on 1-Wire (PB5)
 *
 * Emits over USB-CDC every ~2 s:
 *   SENSOR pH=7.2 temp=23.45 Turbidity=12.3 Voltage=1.6\r\n
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
#define PH_OFFSET_MV 1272.8f    // mV at pH 7.0 (calibrate against buffer)
#define MV_PER_PH    170.4341f  // mV per pH unit  (Nernst ~59.16 mV/pH @ 25 °C)
#define PH_NEUTRAL   7.0f

// ── Turbidity calibration ─────────────────────────────────────────────────────
// SEN0189 parabola has its peak at V ≈ 2.56 V (NTU ≈ 3000),
// and a right-hand zero (clear water) at V ≈ 4.20 V.
// Only the interval 2.56 V < V < 4.20 V is physically valid.
#define NTU_V_PEAK         2.56f
#define NTU_V_CLEAR        4.20f
#define NTU_MAX            3000.0f
#define NTU_EXTRAP_SLOPE   1500.0f   // tweak: extra NTU per V_PEAK below peak

// ── Timer ─────────────────────────────────────────────────────────────────────
void init_timer_settings(void) {
    timer_deinit(TIMER5);

    timer_parameter_struct p;
    p.prescaler         = 107;                  // 108 MHz / (107+1) = 1 MHz tick
    p.alignedmode       = TIMER_COUNTER_EDGE;
    p.counterdirection  = TIMER_COUNTER_UP;
    p.period            = 999;                  // overflow every 1 ms
    p.clockdivision     = TIMER_CKDIV_DIV1;
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

    if (ph < 0.0f)  ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

// ── Turbidity reading ─────────────────────────────────────────────────────────
// Returns NTU.  Higher = more turbid.
// Uses the DFRobot SEN0189 quadratic curve, but only on its valid branch.
// Outside that branch we fall back to clamps / linear extrapolation so the
// reading varies smoothly with very clear and very murky water.
float read_NTU(void)
{
    usb_delay_1ms(10);

    // 12-bit ADC, 0..4095, ref 3.3 V
    float voltage = ((float)adc_values[0] * 3.3f) / 4095.0f;
    // Rescale to sensor's native 0-4.5 V (the formula's coordinate system)
    float v = voltage * (4.5f / 3.3f);

    float ntu;

    if (v >= NTU_V_CLEAR) {
        // Clear water – sensor output near its maximum
        ntu = 0.0f;
    }
    else if (v >= NTU_V_PEAK) {
        // Valid branch of the SEN0189 parabola
        ntu = -1120.4f * v * v + 5742.3f * v - 4352.9f;
        if (ntu < 0.0f)    ntu = 0.0f;
        if (ntu > NTU_MAX) ntu = NTU_MAX;
    }
    else {
        // Below the peak the parabola is non-monotonic and unreliable.
        // Linear extrapolation so very murky samples (e.g. coffee) still
        // produce a varying reading instead of clamping flat at 3000.
        //   v = V_PEAK -> NTU_MAX
        //   v = 0      -> NTU_MAX + NTU_EXTRAP_SLOPE
        ntu = NTU_MAX + (NTU_V_PEAK - v) * (NTU_EXTRAP_SLOPE / NTU_V_PEAK);
        if (ntu > NTU_MAX + NTU_EXTRAP_SLOPE) {
            ntu = NTU_MAX + NTU_EXTRAP_SLOPE;
        }
    }

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
    lio_init_OW();        // initialise 1-Wire on PB5

    configure_usb_serial();

    // Wait for USB host to open the port
    while (!usb_serial_available())
        usb_delay_1ms(100);

    usb_delay_1ms(1000);
    printf("Algae-Halt sensor ready\r\n");
    fflush(0);

    while (1)
    {
        // DEBUG – ta bort när problemet är löst
        printf("RAW[0]=%d RAW[1]=%d\r\n", adc_values[0], adc_values[1]);

        float ph      = read_ph();
        float temp    = read_temp();
        float ntu     = read_NTU();
        float voltage = ((float)adc_values[0] * 3300.0f) / 4095.0f; // turbidity probe voltage (mV)
        int   adc_valueD = adc_values[0];

        // ── Integer / decimal split (no float printf on this libc) ───────────
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
        {
            // Sensor missing: send sentinel so the GUI can show "N/A"
            printf("SENSOR pH=%d.%d temp=NONE Turbidity=%d.%d Voltage=%d.%d ADC: %d \r\n",
                   ph_i, ph_d, ntu_i, ntu_d, voltage_i, voltage_d, adc_valueD);
        }
        else
        {
            int tmp_i = (int)temp;
            int tmp_d = (int)((temp - tmp_i) * 100.0f);
            if (tmp_d < 0) tmp_d = -tmp_d;

            // ── Structured output that the GUI parses ────────────────────────
            // Format: SENSOR pH=<int>.<frac> temp=<int>.<frac2> Turbidity=<int>.<frac> Voltage=<int>.<frac>
            printf("SENSOR pH=%d.%d temp=%d.%02d Turbidity=%d.%d Voltage=%d.%d\r\n",
                   ph_i, ph_d, tmp_i, tmp_d, ntu_i, ntu_d, voltage_i, voltage_d);
        }
        fflush(0);

        usb_delay_1ms(2000); // 2 s between readings
    }
}