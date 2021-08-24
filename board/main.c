// ********************* Includes *********************
#include "config.h"
#include "obj/gitversion.h"

#include "libc.h"

#include "main_declarations.h"

#include "drivers/llcan.h"
#include "drivers/llgpio.h"

#include "board.h"

#include "drivers/debug.h"
#include "drivers/usb.h"
#include "drivers/timer.h"
#include "drivers/clock.h"

#include "gpio.h"

#include "drivers/can.h"

// ********************* Serial debugging *********************

void debug_ring_callback() {
  char rcv;
  while (getc(&rcv)) {
    (void)injectc(rcv);  // misra-c2012-17.7: cast to void is ok: debug function

    // jump to DFU flash
    if (rcv == 'z') {
      enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
      NVIC_SystemReset();
    }

    // normal reset
    if (rcv == 'x') {
      NVIC_SystemReset();
    }
  }
}

// ***************************** USB port *****************************
int usb_cb_ep1_in(void *usbdata, int len, bool hardwired) {
  UNUSED(hardwired);
  CAN_FIFOMailBox_TypeDef *reply = (CAN_FIFOMailBox_TypeDef *)usbdata;
  int ilen = 0;
  while (ilen < MIN(len/0x10, 4) && can_pop(&can_rx_q, &reply[ilen])) {
    ilen++;
  }
  return ilen*0x10;
}

void usb_cb_ep2_out(void *usbdata, int len, bool hardwired) {
  UNUSED(usbdata);
  UNUSED(len);
  UNUSED(hardwired);
}

// send on CAN
void usb_cb_ep3_out(void *usbdata, int len, bool hardwired) {
  UNUSED(hardwired);
  int dpkt = 0;
  uint32_t *d32 = (uint32_t *)usbdata;
  for (dpkt = 0; dpkt < (len / 4); dpkt += 4) {
    CAN_FIFOMailBox_TypeDef to_push;
    to_push.RDHR = d32[dpkt + 3];
    to_push.RDLR = d32[dpkt + 2];
    to_push.RDTR = d32[dpkt + 1];
    to_push.RIR = d32[dpkt];

    uint8_t bus_number = (to_push.RDTR >> 4) & CAN_BUS_NUM_MASK;
    can_send(&to_push, bus_number);
  }
}

void usb_cb_enumeration_complete() {
  puts("USB enumeration complete\n");
  is_enumerated = 1;
}

int usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *resp, bool hardwired) {
  unsigned int resp_len = 0;
  switch (setup->b.bRequest) {
    // **** 0xa0: Set panda power.
    case 0xa0:
      board_set_panda_power((setup->b.wValue.w == 1U));
      break;
    // **** 0xa1: Set harness orientation.
    case 0xa1:
      board_set_harness_orientation(setup->b.wValue.w);
      break;
    // **** 0xa2: Set ignition.
    case 0xa2:
      board_set_ignition((setup->b.wValue.w == 1U));
      break;
    // **** 0xc0: get CAN debug info
    case 0xc0:
      puts("can tx: "); puth(can_tx_cnt);
      puts(" txd: "); puth(can_txd_cnt);
      puts(" rx: "); puth(can_rx_cnt);
      puts(" err: "); puth(can_err_cnt);
      puts("\n");
      break;
    // **** 0xd0: fetch serial number
    case 0xd0:
      (void)memcpy(resp, (uint8_t *)0x1fff79c0, 0x10);
      resp_len = 0x10;
      break;
    // **** 0xd1: enter bootloader mode
    case 0xd1:
      // this allows reflashing of the bootstub
      // so it's blocked over wifi
      switch (setup->b.wValue.w) {
        case 0:
          if (hardwired) {
            puts("-> entering bootloader\n");
            enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
            NVIC_SystemReset();
          }
          break;
        case 1:
          puts("-> entering softloader\n");
          enter_bootloader_mode = ENTER_SOFTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
        default:
          puts("Bootloader mode invalid\n");
          break;
      }
      break;
    // **** 0xd6: get version
    case 0xd6:
      COMPILE_TIME_ASSERT(sizeof(gitversion) <= MAX_RESP_LEN);
      (void)memcpy(resp, gitversion, sizeof(gitversion));
      resp_len = sizeof(gitversion) - 1U;
      break;
    // **** 0xd8: reset ST
    case 0xd8:
      NVIC_SystemReset();
      break;
    // **** 0xdb: set OBD CAN multiplexing mode
    case 0xdb:
      if (setup->b.wValue.w == 1U) {
        // Enable OBD CAN
        board_set_can_mode(CAN_MODE_OBD_CAN2);
      } else {
        // Disable OBD CAN
        board_set_can_mode(CAN_MODE_NORMAL);
      }
      break;
    // **** 0xdd: enable can forwarding
    case 0xdd:
      // wValue = Can Bus Num to forward from
      // wIndex = Can Bus Num to forward to
      if ((setup->b.wValue.w < BUS_MAX) && (setup->b.wIndex.w < BUS_MAX) &&
          (setup->b.wValue.w != setup->b.wIndex.w)) { // set forwarding
        can_set_forwarding(setup->b.wValue.w, setup->b.wIndex.w & CAN_BUS_NUM_MASK);
      } else if((setup->b.wValue.w < BUS_MAX) && (setup->b.wIndex.w == 0xFFU)){ //Clear Forwarding
        can_set_forwarding(setup->b.wValue.w, -1);
      } else {
        puts("Invalid CAN bus forwarding\n");
      }
      break;
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
    default:
      puts("NO HANDLER ");
      puth(setup->b.bRequest);
      puts("\n");
      break;
  }
  return resp_len;
}


// ***************************** main code *****************************

// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void __initialize_hardware_early(void) {
  early();
}

void __attribute__ ((noinline)) enable_fpu(void) {
  // enable the FPU
  SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
}


// called at 10 Hz
// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void TIM3_IRQHandler(void) {
  static uint32_t tcnt = 0;
  static uint32_t button_press_cnt = 0;

  if (TIM3->SR != 0) {
    can_live = pending_can_live;

    // reset this every 10 seconds
    if (tcnt % 100 == 0) {
      pending_can_live = 0;
    }

    if (tcnt % 10 == 0) {
#ifdef DEBUG
      puts("** blink ");
      puth(can_rx_q.r_ptr); puts(" "); puth(can_rx_q.w_ptr); puts("  ");
      puth(can_tx1_q.r_ptr); puts(" "); puth(can_tx1_q.w_ptr); puts("  ");
      puth(can_tx2_q.r_ptr); puts(" "); puth(can_tx2_q.w_ptr); puts("\n");
#endif

      // turn off the blue LED, turned on by CAN
      board_set_led(LED_BLUE, false);

      // Blink and OBD CAN
#ifdef FINAL_PROVISIONING
        board_set_can_mode(can_mode == CAN_MODE_NORMAL ? CAN_MODE_OBD_CAN2 : CAN_MODE_NORMAL);
#endif
    }

    // Check on button
    bool current_button_status = board_get_button();

    if (current_button_status && button_press_cnt == 10) {
      board_set_panda_power(!panda_power);
    }

#ifdef FINAL_PROVISIONING
    // ign on for 0.3s, off for 0.2s
    const bool ign = (tcnt % (3+2)) < 3;
    if (ign != ignition) {
      board_set_ignition(ign);
    }
#else
    static bool prev_button_status = false;
    if (!current_button_status && prev_button_status && button_press_cnt < 10){
      board_set_ignition(!ignition);
    }
    prev_button_status = current_button_status;
#endif

    button_press_cnt = current_button_status ? button_press_cnt + 1 : 0;

    // on to the next one
    tcnt += 1U;
  }
  TIM3->SR = 0;
}

int main(void) {
  // shouldn't have interrupts here, but just in case
  disable_interrupts();

  // init early devices
  clock_init();
  peripherals_init();

  // print hello
  puts("\n\n\n************************ JUNGLE MAIN START ************************\n");

  // init board
  board_init();

  // panda jungle has an FPU, let's use it!
  enable_fpu();

  // init microsecond system timer
  // increments 1000000 times per second
  // generate an update to set the prescaler
  TIM2->PSC = 48-1;
  TIM2->CR1 = TIM_CR1_CEN;
  TIM2->EGR = TIM_EGR_UG;
  // use TIM2->CNT to read

  // Init CAN
  can_init_all();

  // 48mhz / 65536 ~= 732 / 73 = 10
  timer_init(TIM3, 73);
  NVIC_EnableIRQ(TIM3_IRQn);

#ifdef DEBUG
  puts("DEBUG ENABLED\n");
#endif
  // enable USB (right before interrupts or enum can fail!)
  usb_init();

  puts("**** INTERRUPTS ON ****\n");
  enable_interrupts();

  // set a default orientation
  board_set_harness_orientation(HARNESS_ORIENTATION_1);

  // If built with FINAL_PROVISIONING flag, forward all messages on bus 0 and 1 to bus 2
  #ifdef FINAL_PROVISIONING
    puts("---- FINAL PROVISIONING BUILD ---- \n");
    can_set_forwarding(0, 2);
    can_set_forwarding(1, 2);
  #endif


  // LED should keep on blinking all the time
  // useful for debugging, fade breaks = panda jungle is overloaded
  while(true){
    for (int fade = 0; fade < 1024; fade += 8) {
      for (int i = 0; i < 128; i++) {
        board_set_led(LED_RED, 1);
        if (fade < 512) { delay(fade); } else { delay(1024-fade); }
        board_set_led(LED_RED, 0);
        if (fade < 512) { delay(512-fade); } else { delay(fade-512); }
      }
    }
  }

  return 0;
}
