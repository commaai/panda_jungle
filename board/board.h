// ///////////////////////////////////////////////////////////// //
// Hardware abstraction layer for all different supported boards //
// ///////////////////////////////////////////////////////////// //
#include "board_declarations.h"
#include "stm32f4xx_hal_gpio_ex.h"

void gpio_init(void){
  // TODO: Is this block actually doing something???
  // pull low to hold ESP in reset??
  // enable OTG out tied to ground
  GPIOA->ODR = 0;
  GPIOB->ODR = 0;
  GPIOA->PUPDR = 0;
  GPIOB->AFR[0] = 0;
  GPIOB->AFR[1] = 0;

  // A11,A12: USB
  set_gpio_alternate(GPIOA, 11, GPIO_AF10_OTG_FS);
  set_gpio_alternate(GPIOA, 12, GPIO_AF10_OTG_FS);
  GPIOA->OSPEEDR = GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;

  // B8,B9: CAN 1
  set_gpio_alternate(GPIOB, 8, GPIO_AF8_CAN1);
  set_gpio_alternate(GPIOB, 9, GPIO_AF8_CAN1);
}

// Peripheral initialization
void peripherals_init(void){
  // enable GPIOB, UART2, CAN, USB clock
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;

  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN2EN;
  RCC->APB1ENR |= RCC_APB1ENR_CAN3EN;
  RCC->APB1ENR |= RCC_APB1ENR_DACEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // main counter
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;  // slow loop and pedal
  RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;  // gmlan_alt
  //RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
  //RCC->APB1ENR |= RCC_APB1ENR_TIM6EN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
  //RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
}

// Detection with internal pullup
#define PULL_EFFECTIVE_DELAY 10
bool detect_with_pull(GPIO_TypeDef *GPIO, int pin, int mode) {
  set_gpio_mode(GPIO, pin, MODE_INPUT);
  set_gpio_pullup(GPIO, pin, mode);
  for (volatile int i=0; i<PULL_EFFECTIVE_DELAY; i++);
  bool ret = get_gpio_input(GPIO, pin);
  set_gpio_pullup(GPIO, pin, PULL_NONE);
  return ret;
}

// /////// Board functions /////// //

uint8_t harness_orientation = HARNESS_ORIENTATION_NONE;
uint8_t can_mode = CAN_MODE_NORMAL;
bool ignition = false;

void board_enable_can_transciever(uint8_t transciever, bool enabled) {
  switch (transciever){
    case 1U:
      set_gpio_output(GPIOC, 1, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOC, 13, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOA, 0, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 10, !enabled);
      break;
    default:
      puts("Invalid CAN transciever ("); puth(transciever); puts("): enabling failed\n");
      break;
  }
}

void board_enable_can_transcievers(bool enabled) {
  for(uint8_t i=1U; i<=4U; i++){
    board_enable_can_transciever(i, enabled);
  }
}

void board_set_led(uint8_t color, bool enabled) {
  switch (color){
    case LED_RED:
      set_gpio_output(GPIOC, 9, !enabled);
      break;
     case LED_GREEN:
      set_gpio_output(GPIOC, 7, !enabled);
      break;
    case LED_BLUE:
      set_gpio_output(GPIOC, 6, !enabled);
      break;
    default:
      break;
  }
}

void board_set_can_mode(uint8_t mode) {
  switch (mode) {
    case CAN_MODE_NORMAL:
      puts("Setting normal CAN mode\n");
      // B12,B13: disable OBD mode
      set_gpio_mode(GPIOB, 12, MODE_INPUT);
      set_gpio_mode(GPIOB, 13, MODE_INPUT);

      // B5,B6: normal CAN2 mode
      set_gpio_alternate(GPIOB, 5, GPIO_AF9_CAN2);
      set_gpio_alternate(GPIOB, 6, GPIO_AF9_CAN2);
      can_mode = CAN_MODE_NORMAL;
      break;
    case CAN_MODE_OBD_CAN2:
      puts("Setting OBD CAN mode\n");
      // B5,B6: disable normal CAN2 mode
      set_gpio_mode(GPIOB, 5, MODE_INPUT);
      set_gpio_mode(GPIOB, 6, MODE_INPUT);

      // B12,B13: OBD mode
      set_gpio_alternate(GPIOB, 12, GPIO_AF9_CAN2);
      set_gpio_alternate(GPIOB, 13, GPIO_AF9_CAN2);
      can_mode = CAN_MODE_OBD_CAN2;
      break;
    default:
      puts("Tried to set unsupported CAN mode: "); puth(mode); puts("\n");
      break;
  }
}

void board_set_harness_orientation(uint8_t orientation) {
  switch (orientation) {
    case HARNESS_ORIENTATION_NONE:
      set_gpio_output(GPIOA, 2, false);
      set_gpio_output(GPIOA, 3, false);
      set_gpio_output(GPIOA, 4, false);
      set_gpio_output(GPIOA, 5, false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_1:
      set_gpio_output(GPIOA, 2, false);
      set_gpio_output(GPIOA, 3, ignition);
      set_gpio_output(GPIOA, 4, true);
      set_gpio_output(GPIOA, 5, false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_2:
      set_gpio_output(GPIOA, 2, ignition);
      set_gpio_output(GPIOA, 3, false);
      set_gpio_output(GPIOA, 4, false);
      set_gpio_output(GPIOA, 5, true);
      harness_orientation = orientation;
      break;
    default:
      puts("Tried to set an unsupported harness orientation: "); puth(orientation); puts("\n");
      break;
  }
}

void board_set_ignition(bool ign) {
  ignition = ign;

  // 'Reset' harness orientation to update GPIO
  board_set_harness_orientation(harness_orientation);
}

bool panda_power = false;
void board_set_panda_power(bool enable) {
  panda_power = enable;
  set_gpio_output(GPIOB, 14, enable);
}

bool board_get_button(void) {
  return get_gpio_input(GPIOC, 8);
}

// ///// Board initialization ///// //
void board_init(void) {
  gpio_init();

  // A8,A15: normal CAN3 mode
  set_gpio_alternate(GPIOA, 8, GPIO_AF11_CAN3);
  set_gpio_alternate(GPIOA, 15, GPIO_AF11_CAN3);

  // Enable CAN transcievers
  board_enable_can_transcievers(true);

  // Disable LEDs
  board_set_led(LED_RED, false);
  board_set_led(LED_GREEN, false);
  board_set_led(LED_BLUE, false);

  // Set normal CAN mode
  board_set_can_mode(CAN_MODE_NORMAL);

  // Set to no harness orientation
  board_set_harness_orientation(HARNESS_ORIENTATION_NONE);

  // Enable panda power by default
  board_set_panda_power(true);
}
