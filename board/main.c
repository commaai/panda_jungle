// ********************* Includes *********************
#include "config.h"

#include "drivers/pwm.h"
#include "drivers/usb.h"
#include "drivers/debug.h"
#include "drivers/gmlan_alt.h"
#include "drivers/simple_watchdog.h"

#include "early_init.h"
#include "provision.h"

#include "health.h"

#include "drivers/can_common.h"

#ifdef STM32H7
  #include "drivers/fdcan.h"
#else
  #include "drivers/bxcan.h"
#endif

#include "obj/gitversion.h"

#include "can_comms.h"
#include "main_comms.h"


// ********************* Serial debugging *********************

void debug_ring_callback(void) {
  char rcv;
  while (getc(&rcv)) {
    (void)injectc(rcv);  // misra-c2012-17.7: cast to void is ok: debug function

    // only allow bootloader entry on debug builds
    #ifdef ALLOW_DEBUG
      // jump to DFU flash
      if (rcv == 'z') {
        enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
        NVIC_SystemReset();
      }
    #endif

    // normal reset
    if (rcv == 'x') {
      NVIC_SystemReset();
    }
  }
}

<<<<<<< HEAD
// ****************************** safety mode ******************************

// this is the only way to leave silent mode
void set_safety_mode(uint16_t mode, uint16_t param) {
  uint16_t mode_copy = mode;
  int err = set_safety_hooks(mode_copy, param);
  if (err == -1) {
    print("Error: safety set mode failed. Falling back to SILENT\n");
    mode_copy = SAFETY_SILENT;
    err = set_safety_hooks(mode_copy, 0U);
    if (err == -1) {
      print("Error: Failed setting SILENT mode. Hanging\n");
      while (true) {
        // TERMINAL ERROR: we can't continue if SILENT safety mode isn't succesfully set
      }
    }
  }
  safety_tx_blocked = 0;
  safety_rx_invalid = 0;

  switch (mode_copy) {
    case SAFETY_SILENT:
      set_intercept_relay(false);
      if (current_board->has_obd) {
        current_board->set_can_mode(CAN_MODE_NORMAL);
      }
      can_silent = ALL_CAN_SILENT;
      break;
    case SAFETY_NOOUTPUT:
      set_intercept_relay(false);
      if (current_board->has_obd) {
        current_board->set_can_mode(CAN_MODE_NORMAL);
      }
      can_silent = ALL_CAN_LIVE;
      break;
    case SAFETY_ELM327:
      set_intercept_relay(false);
      heartbeat_counter = 0U;
      heartbeat_lost = false;
      if (current_board->has_obd) {
        if (param == 0U) {
          current_board->set_can_mode(CAN_MODE_OBD_CAN2);
        } else {
          current_board->set_can_mode(CAN_MODE_NORMAL);
        }
      }
      can_silent = ALL_CAN_LIVE;
      break;
<<<<<<< HEAD
    // **** 0xde: set can bitrate
    case 0xde:
      if (setup->b.wValue.w < BUS_MAX) {
        can_speed[setup->b.wValue.w] = setup->b.wIndex.w;
        can_init(CAN_NUM_FROM_BUS_NUM(setup->b.wValue.w));
      }
      break;
    // **** 0xe0: debug read
    case 0xe0:
      // read
      while ((resp_len < MIN(setup->b.wLength.w, MAX_RESP_LEN)) &&
                         getc((char*)&resp[resp_len])) {
        ++resp_len;
      }
      break;
    // **** 0xe5: set CAN loopback (for testing)
    case 0xe5:
      can_loopback = (setup->b.wValue.w > 0U);
      can_init_all();
      break;
    // **** 0xf1: Clear CAN ring buffer.
    case 0xf1:
      if (setup->b.wValue.w == 0xFFFFU) {
        puts("Clearing CAN Rx queue\n");
        can_clear(&can_rx_q);
      } else if (setup->b.wValue.w < BUS_MAX) {
        puts("Clearing CAN Tx queue\n");
        can_clear(can_queues[setup->b.wValue.w]);
      } else {
        puts("Clearing CAN CAN ring buffer failed: wrong bus number\n");
      }
      break;
    // **** 0xf2: Clear debug ring buffer.
    case 0xf2:
      {
        puts("Clearing debug queue.\n");
        clear_debug_buff();
        break;
      }
    // **** 0xf4: Set CAN transceiver enable pin
    case 0xf4:
      board_enable_can_transciever(setup->b.wValue.w, setup->b.wIndex.w > 0U);
      break;
    // **** 0xf5: Set CAN silent mode
    case 0xf5:
      can_silent = (setup->b.wValue.w > 0U) ? ALL_CAN_SILENT : ALL_CAN_LIVE;
      can_init_all();
      break;
=======
>>>>>>> e7c4b5d (copy latest panda fw (adc0c12))
    default:
      set_intercept_relay(true);
      heartbeat_counter = 0U;
      heartbeat_lost = false;
      if (current_board->has_obd) {
        current_board->set_can_mode(CAN_MODE_NORMAL);
      }
      can_silent = ALL_CAN_LIVE;
      break;
  }
  can_init_all();
}

bool is_car_safety_mode(uint16_t mode) {
  return (mode != SAFETY_SILENT) &&
         (mode != SAFETY_NOOUTPUT) &&
         (mode != SAFETY_ALLOUTPUT) &&
         (mode != SAFETY_ELM327);
}

=======
>>>>>>> 7043877 (no build errors)
// ***************************** main code *****************************

// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void __initialize_hardware_early(void) {
  early_initialization();
}

void __attribute__ ((noinline)) enable_fpu(void) {
  // enable the FPU
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}

<<<<<<< HEAD
// go into SILENT when heartbeat isn't received for this amount of seconds.
#define HEARTBEAT_IGNITION_CNT_ON 5U
#define HEARTBEAT_IGNITION_CNT_OFF 2U

<<<<<<< HEAD
// called at 10 Hz
// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void TIM3_IRQHandler(void) {
  static uint32_t tcnt = 0;
  static uint32_t button_press_cnt = 0;
=======
=======
>>>>>>> 7043877 (no build errors)
// called at 8Hz
uint8_t loop_counter = 0U;
void tick_handler(void) {
  if (TICK_TIMER->SR != 0) {
<<<<<<< HEAD
    // siren
    current_board->set_siren((loop_counter & 1U) && (siren_enabled || (siren_countdown > 0U)));
>>>>>>> e7c4b5d (copy latest panda fw (adc0c12))
=======
>>>>>>> 7043877 (no build errors)

    // tick drivers at 8Hz
    usb_tick();
    simple_watchdog_kick();

    // decimated to 1Hz
    if (loop_counter == 0U) {
      #ifdef DEBUG
        print("** blink ");
        print("rx:"); puth4(can_rx_q.r_ptr); print("-"); puth4(can_rx_q.w_ptr); print("  ");
        print("tx1:"); puth4(can_tx1_q.r_ptr); print("-"); puth4(can_tx1_q.w_ptr); print("  ");
        print("tx2:"); puth4(can_tx2_q.r_ptr); print("-"); puth4(can_tx2_q.w_ptr); print("  ");
        print("tx3:"); puth4(can_tx3_q.r_ptr); print("-"); puth4(can_tx3_q.w_ptr); print("\n");
      #endif

      current_board->board_tick();

      // check registers
      check_registers();

      // on to the next one
      uptime_cnt += 1U;
    }

    // Check on button
    bool current_button_status = current_board->get_button();

    if (current_button_status && button_press_cnt == 10) {
      current_board->set_panda_power(!panda_power);
    }

#ifdef FINAL_PROVISIONING
    // ign on for 0.3s, off for 0.2s
    const bool ign = (tcnt % (3+2)) < 3;
    if (ign != ignition) {
      current_board->set_ignition(ign);
    }
#else
    static bool prev_button_status = false;
    if (!current_button_status && prev_button_status && button_press_cnt < 10){
      current_board->set_ignition(!ignition);
    }
    prev_button_status = current_button_status;
#endif

    button_press_cnt = current_button_status ? button_press_cnt + 1 : 0;

    loop_counter++;
    loop_counter %= 8U;
  }
  TICK_TIMER->SR = 0;
}


int main(void) {
  // Init interrupt table
  init_interrupts(true);

  // shouldn't have interrupts here, but just in case
  disable_interrupts();

  // init early devices
  clock_init();
  peripherals_init();
  detect_board_type();

  // print hello
  print("\n\n\n************************ MAIN START ************************\n");

  // check for non-supported board types
  if(hw_type == HW_TYPE_UNKNOWN){
    print("Unsupported board type\n");
    while (1) { /* hang */ }
  }

  print("Config:\n");
  print("  Board type: "); print(current_board->board_type); print("\n");

  // init board
  current_board->init();

  // we have an FPU, let's use it!
  enable_fpu();

  microsecond_timer_init();

  // init watchdog for interrupt loop, fed at 8Hz
  simple_watchdog_init(FAULT_HEARTBEAT_LOOP_WATCHDOG, (3U * 1000000U / 8U));

  // 8Hz timer
  REGISTER_INTERRUPT(TICK_TIMER_IRQ, tick_handler, 10U, FAULT_INTERRUPT_RATE_TICK)
  tick_timer_init();

#ifdef DEBUG
  print("DEBUG ENABLED\n");
#endif
  // enable USB (right before interrupts or enum can fail!)
  usb_init();

  print("**** INTERRUPTS ON ****\n");
  enable_interrupts();

  // LED should keep on blinking all the time
  uint64_t cnt = 0;

  for (cnt=0;;cnt++) {
    #ifdef DEBUG_FAULTS
    if (fault_status == FAULT_STATUS_NONE) {
    #endif
      // useful for debugging, fade breaks = panda is overloaded
      for (uint32_t fade = 0U; fade < MAX_LED_FADE; fade += 1U) {
        current_board->set_led(LED_RED, true);
        delay(fade >> 4);
        current_board->set_led(LED_RED, false);
        delay((MAX_LED_FADE - fade) >> 4);
      }

      for (uint32_t fade = MAX_LED_FADE; fade > 0U; fade -= 1U) {
        current_board->set_led(LED_RED, true);
        delay(fade >> 4);
        current_board->set_led(LED_RED, false);
        delay((MAX_LED_FADE - fade) >> 4);
      }

    #ifdef DEBUG_FAULTS
    } else {
        current_board->set_led(LED_RED, 1);
        delay(512000U);
        current_board->set_led(LED_RED, 0);
        delay(512000U);
      }
    #endif
  }

  return 0;
}
