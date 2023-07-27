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

// ***************************** main code *****************************

// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void __initialize_hardware_early(void) {
  early_initialization();
}

void __attribute__ ((noinline)) enable_fpu(void) {
  // enable the FPU
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}

// called at 8Hz
uint8_t loop_counter = 0U;
uint16_t button_press_cnt = 0U;
void tick_handler(void) {
  if (TICK_TIMER->SR != 0) {
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

      // turn off the blue LED, turned on by CAN
      current_board->set_led(LED_BLUE, false);

      // Blink and OBD CAN
#ifdef FINAL_PROVISIONING
      current_board->set_can_mode(can_mode == CAN_MODE_NORMAL ? CAN_MODE_OBD_CAN2 : CAN_MODE_NORMAL);
#endif

      // on to the next one
      uptime_cnt += 1U;
    }

    current_board->set_led(LED_GREEN, green_led_enabled);

    // Check on button
    bool current_button_status = current_board->get_button();

    if (current_button_status && button_press_cnt == 10) {
      current_board->set_panda_power(!panda_power);
    }

#ifdef FINAL_PROVISIONING
    // ign on for 0.3s, off for 0.2s
    const bool ign = (loop_counter % (3+2)) < 3;
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

  can_init_all();
  current_board->set_harness_orientation(HARNESS_ORIENTATION_1);

#ifdef FINAL_PROVISIONING
  print("---- FINAL PROVISIONING BUILD ---- \n");
  can_set_forwarding(0, 2);
  can_set_forwarding(1, 2);
#endif

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
