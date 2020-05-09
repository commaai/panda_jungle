#define BOOTSTUB

#include "config.h"
#include "obj/gitversion.h"

#include "stm32f4xx.h"
#include "stm32f4xx_hal_gpio_ex.h"

// ******************** Prototypes ********************
void puts(const char *a){ UNUSED(a); }
void puth(unsigned int i){ UNUSED(i); }
void puth2(unsigned int i){ UNUSED(i); }

// No CAN support on bootloader
void can_flip_buses(uint8_t bus1, uint8_t bus2){UNUSED(bus1); UNUSED(bus2);}

// ********************* Includes *********************
#include "libc.h"

#include "drivers/clock.h"
#include "drivers/llgpio.h"

#include "board.h"

#include "gpio.h"

#include "drivers/usb.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "spi_flasher.h"
#include "obj/cert.h"

void __initialize_hardware_early(void) {
  early();
}

void fail(void) {
  soft_flasher_start();
}

// know where to sig check
extern void *_app_start[];

int main(void) {
  disable_interrupts();
  clock_init();

  if (enter_bootloader_mode == ENTER_SOFTLOADER_MAGIC) {
    enter_bootloader_mode = 0;
    soft_flasher_start();
  }

  // validate length
  int len = (int)_app_start[0];
  if ((len < 8) || (len > (0x1000000 - 0x4000 - 4 - RSANUMBYTES))) goto fail;

  // compute SHA hash
  uint8_t digest[SHA_DIGEST_SIZE];
  SHA_hash(&_app_start[1], len-4, digest);

  // verify RSA signature
  if (RSA_verify(&release_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }

  // allow debug if built from source
#ifdef ALLOW_DEBUG
  if (RSA_verify(&debug_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }
#endif

// here is a failure
fail:
  fail();
  return 0;
good:
  // jump to flash
  ((void(*)(void)) _app_start[1])();
  return 0;
}

