void gpio_usb_init(void) {
  // A11,A12: USB
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG1_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG1_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12;
}

void gpio_spi_init(void) {
  set_gpio_alternate(GPIOE, 11, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 12, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 13, GPIO_AF5_SPI4);
  set_gpio_alternate(GPIOE, 14, GPIO_AF5_SPI4);
  register_set_bits(&(GPIOE->OSPEEDR), GPIO_OSPEEDR_OSPEED11 | GPIO_OSPEEDR_OSPEED12 | GPIO_OSPEEDR_OSPEED13 | GPIO_OSPEEDR_OSPEED14);
}

void gpio_usart2_init(void) {
  // A2,A3: USART 2 for debugging
  set_gpio_alternate(GPIOA, 2, GPIO_AF7_USART2);
  set_gpio_alternate(GPIOA, 3, GPIO_AF7_USART2);
}

void gpio_uart7_init(void) {
  // E7,E8: UART 7 for debugging
  set_gpio_alternate(GPIOE, 7, GPIO_AF7_UART7);
  set_gpio_alternate(GPIOE, 8, GPIO_AF7_UART7);
}

// Common GPIO initialization
void common_init_gpio(void) {
  gpio_usb_init();

  // B8,B9: FDCAN1
  set_gpio_pullup(GPIOB, 8, PULL_NONE);
  set_gpio_alternate(GPIOB, 8, GPIO_AF9_FDCAN1);

  set_gpio_pullup(GPIOB, 9, PULL_NONE);
  set_gpio_alternate(GPIOB, 9, GPIO_AF9_FDCAN1);

  // B5,B6 (mplex to B12,B13): FDCAN2
  set_gpio_pullup(GPIOB, 12, PULL_NONE);
  set_gpio_pullup(GPIOB, 13, PULL_NONE);

  set_gpio_pullup(GPIOB, 5, PULL_NONE);
  set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

  set_gpio_pullup(GPIOB, 6, PULL_NONE);
  set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);

  // G9,G10: FDCAN3
  set_gpio_pullup(GPIOG, 9, PULL_NONE);
  set_gpio_alternate(GPIOG, 9, GPIO_AF2_FDCAN3);

  set_gpio_pullup(GPIOG, 10, PULL_NONE);
  set_gpio_alternate(GPIOG, 10, GPIO_AF2_FDCAN3);
}

void flasher_peripherals_init(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN;

  // SPI + DMA
  RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
}

// Peripheral initialization
void peripherals_init(void) {
  // enable GPIO(A,B,C,D,E,F,G,H)
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOBEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOCEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOFEN;
  RCC->AHB4ENR |= RCC_AHB4ENR_GPIOGEN;

  RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;  // SPI
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;  // DAC DMA
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;  // SPI DMA
  RCC->APB1LENR |= RCC_APB1LENR_TIM2EN;  // main counter
  RCC->APB1LENR |= RCC_APB1LENR_TIM3EN;  // fan pwm
  RCC->APB1LENR |= RCC_APB1LENR_TIM6EN;  // interrupt timer
  RCC->APB1LENR |= RCC_APB1LENR_TIM7EN;  // DMA trigger timer
  RCC->APB1LENR |= RCC_APB1LENR_UART7EN;  // SOM uart
  RCC->APB1LENR |= RCC_APB1LENR_DAC12EN; // DAC
  RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;  // tick timer
  RCC->APB1LENR |= RCC_APB1LENR_TIM12EN;  // slow loop
  RCC->APB1LENR |= RCC_APB1LENR_I2C5EN;  // codec I2C
  RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;  // clock source timer
  RCC->AHB3ENR |= RCC_AHB3ENR_SDMMC1EN; // SDMMC

  RCC->APB1HENR |= RCC_APB1HENR_FDCANEN; // FDCAN core enable
  RCC->AHB1ENR |= RCC_AHB1ENR_ADC12EN; // Enable ADC12 clocks
  RCC->AHB4ENR |= RCC_AHB4ENR_ADC3EN; // Enable ADC3 clocks

  RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;

  // HS USB enable, also LP is needed for CSleep state(__WFI())
  RCC->AHB1ENR |= RCC_AHB1ENR_USB1OTGHSEN;
  RCC->AHB1LPENR |= RCC_AHB1LPENR_USB1OTGHSLPEN;
  RCC->AHB1LPENR &= ~(RCC_AHB1LPENR_USB1OTGHSULPILPEN);
}

void enable_interrupt_timer(void) {
  register_set_bits(&(RCC->APB1LENR), RCC_APB1LENR_TIM6EN); // Enable interrupt timer peripheral
}
