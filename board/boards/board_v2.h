
void board_v2_set_led(uint8_t color, bool enabled) {
  UNUSED(color);
  UNUSED(enabled);
}

void board_v2_set_can_mode(uint8_t mode) {
  UNUSED(mode);
}

void board_v2_set_harness_orientation(uint8_t orientation) {
  UNUSED(orientation);
}

bool panda_power = false;
void board_v2_set_panda_power(bool enable) {
  panda_power = enable;
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
};