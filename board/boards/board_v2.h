
const gpio_t power_pins[] = {
  {.bank = GPIOA, .pin = 0},
  {.bank = GPIOA, .pin = 1},
  {.bank = GPIOF, .pin = 12},
  {.bank = GPIOA, .pin = 5},
  {.bank = GPIOC, .pin = 5},
  {.bank = GPIOB, .pin = 2},
};

const gpio_t sbu1_ignition_pins[] = {
  {.bank = GPIOD, .pin = 0},
  {.bank = GPIOD, .pin = 5},
  {.bank = GPIOD, .pin = 12},
  {.bank = GPIOD, .pin = 14},
  {.bank = GPIOE, .pin = 5},
  {.bank = GPIOE, .pin = 9},
};

const gpio_t sbu1_relay_pins[] = {
  {.bank = GPIOD, .pin = 1},
  {.bank = GPIOD, .pin = 6},
  {.bank = GPIOD, .pin = 11},
  {.bank = GPIOD, .pin = 15},
  {.bank = GPIOE, .pin = 6},
  {.bank = GPIOE, .pin = 10},
};

const gpio_t sbu2_ignition_pins[] = {
  {.bank = GPIOD, .pin = 3},
  {.bank = GPIOD, .pin = 8},
  {.bank = GPIOD, .pin = 9},
  {.bank = GPIOE, .pin = 0},
  {.bank = GPIOE, .pin = 7},
  {.bank = GPIOE, .pin = 11},
};

const gpio_t sbu2_relay_pins[] = {
  {.bank = GPIOD, .pin = 4},
  {.bank = GPIOD, .pin = 10},
  {.bank = GPIOD, .pin = 13},
  {.bank = GPIOE, .pin = 1},
  {.bank = GPIOE, .pin = 8},
  {.bank = GPIOE, .pin = 12},
};

void board_v2_set_led(uint8_t color, bool enabled) {
  switch (color) {
    case LED_RED:
      set_gpio_output(GPIOE, 4, !enabled);
      break;
     case LED_GREEN:
      set_gpio_output(GPIOE, 3, !enabled);
      break;
    case LED_BLUE:
      set_gpio_output(GPIOE, 2, !enabled);
      break;
    default:
      break;
  }
}

void board_v2_set_harness_orientation(uint8_t orientation) {
  switch (orientation) {
    case HARNESS_ORIENTATION_NONE:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_1:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), true);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), false);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_2:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), true);
      harness_orientation = orientation;
      break;
    default:
      print("Tried to set an unsupported harness orientation: "); puth(orientation); print("\n");
      break;
  }
}

void board_v2_set_can_mode(uint8_t mode) {
  switch (mode) {
    case CAN_MODE_NORMAL:
      // B12,B13: disable normal mode
      set_gpio_pullup(GPIOB, 12, PULL_NONE);
      set_gpio_mode(GPIOB, 12, MODE_ANALOG);

      set_gpio_pullup(GPIOB, 13, PULL_NONE);
      set_gpio_mode(GPIOB, 13, MODE_ANALOG);

      // B5,B6: FDCAN2 mode
      set_gpio_pullup(GPIOB, 5, PULL_NONE);
      set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

      set_gpio_pullup(GPIOB, 6, PULL_NONE);
      set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
      break;
    case CAN_MODE_OBD_CAN2:
      // B5,B6: disable normal mode
      set_gpio_pullup(GPIOB, 5, PULL_NONE);
      set_gpio_mode(GPIOB, 5, MODE_ANALOG);

      set_gpio_pullup(GPIOB, 6, PULL_NONE);
      set_gpio_mode(GPIOB, 6, MODE_ANALOG);
      // B12,B13: FDCAN2 mode
      set_gpio_pullup(GPIOB, 12, PULL_NONE);
      set_gpio_alternate(GPIOB, 12, GPIO_AF9_FDCAN2);

      set_gpio_pullup(GPIOB, 13, PULL_NONE);
      set_gpio_alternate(GPIOB, 13, GPIO_AF9_FDCAN2);
      break;
    default:
      break;
  }
}

bool panda_power = false;
void board_v2_set_panda_power(bool enable) {
  panda_power = enable;
  gpio_set_all_output(power_pins, sizeof(power_pins) / sizeof(gpio_t), !enable);
}

bool board_v2_get_button(void) {
  return get_gpio_input(GPIOG, 15);
}

void board_v2_set_ignition(bool enabled) {
  ignition = enabled;
  board_v2_set_harness_orientation(harness_orientation);
}

void board_v2_enable_can_transciever(uint8_t transciever, bool enabled) {
  switch (transciever){
    case 1U:
      set_gpio_output(GPIOG, 11, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOB, 3, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOD, 7, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 4, !enabled);
      break;
    default:
      print("Invalid CAN transciever ("); puth(transciever); print("): enabling failed\n");
      break;
  }
}

void board_v2_init(void) {
  common_init_gpio();

  // Disable LEDs
  board_v2_set_led(LED_RED, false);
  board_v2_set_led(LED_GREEN, false);
  board_v2_set_led(LED_BLUE, false);

  // Normal CAN mode
  board_v2_set_can_mode(CAN_MODE_NORMAL);

  // Enable CAN transcievers
  for(uint8_t i = 1; i <= 4; i++) {
    board_v2_enable_can_transciever(i, true);
  }

  // Set to no harness orientation
  board_v2_set_harness_orientation(HARNESS_ORIENTATION_NONE);

  // Enable panda power by default
  board_v2_set_panda_power(true);
}

void board_v2_tick(void) {}

const board board_v2 = {
  .board_type = "V2",
  .has_canfd = true,
  .avdd_mV = 1800U,
  .init = &board_v2_init,
  .set_led = &board_v2_set_led,
  .board_tick = &board_v2_tick,
  .get_button = &board_v2_get_button,
  .set_panda_power = &board_v2_set_panda_power,
  .set_ignition = &board_v2_set_ignition,
  .set_harness_orientation = &board_v2_set_harness_orientation,
  .set_can_mode = &board_v2_set_can_mode,
  .enable_can_transciever = &board_v2_enable_can_transciever
};