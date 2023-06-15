
void board_v1_set_led(uint8_t color, bool enabled) {
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

void board_v1_set_can_mode(uint8_t mode) {
  switch (mode) {
    case CAN_MODE_NORMAL:
      print("Setting normal CAN mode\n");
      // B12,B13: disable OBD mode
      set_gpio_mode(GPIOB, 12, MODE_INPUT);
      set_gpio_mode(GPIOB, 13, MODE_INPUT);

      // B5,B6: normal CAN2 mode
      set_gpio_alternate(GPIOB, 5, GPIO_AF9_CAN2);
      set_gpio_alternate(GPIOB, 6, GPIO_AF9_CAN2);
      can_mode = CAN_MODE_NORMAL;
      break;
    case CAN_MODE_OBD_CAN2:
      print("Setting OBD CAN mode\n");
      // B5,B6: disable normal CAN2 mode
      set_gpio_mode(GPIOB, 5, MODE_INPUT);
      set_gpio_mode(GPIOB, 6, MODE_INPUT);

      // B12,B13: OBD mode
      set_gpio_alternate(GPIOB, 12, GPIO_AF9_CAN2);
      set_gpio_alternate(GPIOB, 13, GPIO_AF9_CAN2);
      can_mode = CAN_MODE_OBD_CAN2;
      break;
    default:
      print("Tried to set unsupported CAN mode: "); puth(mode); print("\n");
      break;
  }
}

void board_v1_set_harness_orientation(uint8_t orientation) {
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
      print("Tried to set an unsupported harness orientation: "); puth(orientation); print("\n");
      break;
  }
}

bool panda_power = false;
void board_v1_set_panda_power(bool enable) {
  panda_power = enable;
  set_gpio_output(GPIOB, 14, enable);
}

bool board_v1_get_button(void) {
  return get_gpio_input(GPIOC, 8);
}

void board_v1_set_ignition(bool enabled) {
  ignition = enabled;
  board_v1_set_harness_orientation(harness_orientation);
}

void board_v1_enable_can_transciever(uint8_t transciever, bool enabled) {
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
      print("Invalid CAN transciever ("); puth(transciever); print("): enabling failed\n");
      break;
  }
}

void board_v1_init(void) {
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

  board_v1_set_can_mode(CAN_MODE_NORMAL);

  // Enable CAN transcievers
  for(uint8_t i = 1; i <= 4; i++) {
    board_v1_enable_can_transciever(i, true);
  }

  // Disable LEDs
  board_v1_set_led(LED_RED, false);
  board_v1_set_led(LED_GREEN, false);
  board_v1_set_led(LED_BLUE, false);

  // Set normal CAN mode
  board_v1_set_can_mode(CAN_MODE_NORMAL);

  // Set to no harness orientation
  board_v1_set_harness_orientation(HARNESS_ORIENTATION_NONE);

  // Enable panda power by default
  board_v1_set_panda_power(true);
}

void board_v1_tick(void) {}

const board board_v1 = {
  .board_type = "V1",
  .has_canfd = false,
  .avdd_mV = 3300U,
  .init = &board_v1_init,
  .set_led = &board_v1_set_led,
  .board_tick = &board_v1_tick,
  .get_button = &board_v1_get_button,
  .set_panda_power = &board_v1_set_panda_power,
  .set_ignition = &board_v1_set_ignition,
  .set_harness_orientation = &board_v1_set_harness_orientation,
  .set_can_mode = &board_v1_set_can_mode,
  .enable_can_transciever = &board_v1_enable_can_transciever
};