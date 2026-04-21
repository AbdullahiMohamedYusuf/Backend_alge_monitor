#include <stdint.h>  // ← add this

void ADC3powerUpInit(int tmp);    // ADC0 Ch 3, and if tmp also ch 16
extern uint16_t adc_values[2];  // ← add this