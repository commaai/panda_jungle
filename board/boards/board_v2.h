
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

void board_v2_set_can_mode(uint8_t mode) {
  UNUSED(mode);
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
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), true);
      harness_orientation = orientation;
      break;
    case HARNESS_ORIENTATION_2:
      gpio_set_all_output(sbu1_ignition_pins, sizeof(sbu1_ignition_pins) / sizeof(gpio_t), false);
      gpio_set_all_output(sbu1_relay_pins, sizeof(sbu1_relay_pins) / sizeof(gpio_t), true);
      gpio_set_all_output(sbu2_ignition_pins, sizeof(sbu2_ignition_pins) / sizeof(gpio_t), ignition);
      gpio_set_all_output(sbu2_relay_pins, sizeof(sbu2_relay_pins) / sizeof(gpio_t), false);
      harness_orientation = orientation;
      break;
    default:
      print("Tried to set an unsupported harness orientation: "); puth(orientation); print("\n");
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

void board_v2_init(void) {
  // Disable LEDs
  board_v2_set_led(LED_RED, false);
  board_v2_set_led(LED_GREEN, false);
  board_v2_set_led(LED_BLUE, false);

  // Set normal CAN mode
  board_v2_set_can_mode(CAN_MODE_NORMAL);

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
  .set_harness_orientation = &board_v2_set_harness_orientation
};