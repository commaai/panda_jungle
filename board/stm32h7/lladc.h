// 5VOUT_S = ADC12_INP5
// VOLT_S = ADC1_INP2
#define ADCCHAN_VIN 2

void adc_init(void) {
  ADC1->CR &= ~(ADC_CR_DEEPPWD); //Reset deep-power-down mode
  ADC1->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
  while(!(ADC1->ISR & ADC_ISR_LDORDY));

  ADC1->CR &= ~(ADC_CR_ADCALDIF); // Choose single-ended calibration
  ADC1->CR |= ADC_CR_ADCALLIN; // Lineriality calibration
  ADC1->CR |= ADC_CR_ADCAL; // Start calibrtation
  while((ADC1->CR & ADC_CR_ADCAL) != 0);

  ADC1->ISR |= ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  while(!(ADC1->ISR & ADC_ISR_ADRDY));
}

uint16_t adc_get_raw(uint8_t channel) {

  ADC1->SQR1 &= ~(ADC_SQR1_L);
  ADC1->SQR1 = ((uint32_t) channel << 6U);

  ADC1->SMPR1 = (0x7U << (channel * 3U) );
  ADC1->PCSEL_RES0 = (0x1U << channel);

  ADC1->CR |= ADC_CR_ADSTART;
  while (!(ADC1->ISR & ADC_ISR_EOC));

  uint16_t res = ADC1->DR;

  while (!(ADC1->ISR & ADC_ISR_EOS));
  ADC1->ISR |= ADC_ISR_EOS;

  return res;
}

uint16_t adc_get_mV(uint8_t channel) {
  return (adc_get_raw(channel) * current_board->avdd_mV) / 65535U;
}
