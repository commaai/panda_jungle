// ***************************** Definitions *****************************
#define BUFFER_SIZE 0x400U

typedef struct debug_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  uint32_t tx_fifo_size;
  volatile uint16_t w_ptr_rx;
  volatile uint16_t r_ptr_rx;
  uint8_t *elems_rx;
  uint32_t rx_fifo_size;
  void (*callback)(void);
} debug_ring;

#define DEBUG_BUFFER(x, size_rx, size_tx, callback_ptr) \
  uint8_t elems_rx_##x[size_rx]; \
  uint8_t elems_tx_##x[size_tx]; \
  debug_ring ring_##x = {  \
    .w_ptr_tx = 0, \
    .r_ptr_tx = 0, \
    .elems_tx = ((uint8_t *)&elems_tx_##x), \
    .tx_fifo_size = size_tx, \
    .w_ptr_rx = 0, \
    .r_ptr_rx = 0, \
    .elems_rx = ((uint8_t *)&elems_rx_##x), \
    .rx_fifo_size = size_rx, \
    .callback = callback_ptr \
  };

// ***************************** Function prototypes *****************************
bool getc(char *elem);
bool putc(char elem);

void puts(const char *a);
void puth(unsigned int i);
void hexdump(const void *a, int l);

void debug_ring_callback(void);

// ******************************* Buffer creation ******************************
DEBUG_BUFFER(debug, BUFFER_SIZE, BUFFER_SIZE, debug_ring_callback)

// ************************* Low-level buffer functions *************************

bool getc(char *elem) {
  bool ret = false;

  ENTER_CRITICAL();
  if (ring_debug.w_ptr_rx != ring_debug.r_ptr_rx) {
    if (elem != NULL) *elem = ring_debug.elems_rx[ring_debug.r_ptr_rx];
    ring_debug.r_ptr_rx = (ring_debug.r_ptr_rx + 1U) % ring_debug.rx_fifo_size;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

bool injectc(char elem) {
  int ret = false;
  uint16_t next_w_ptr;

  ENTER_CRITICAL();
  next_w_ptr = (ring_debug.w_ptr_rx + 1U) % ring_debug.tx_fifo_size;
  if (next_w_ptr != ring_debug.r_ptr_rx) {
    ring_debug.elems_rx[ring_debug.w_ptr_rx] = elem;
    ring_debug.w_ptr_rx = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

void clear_debug_buff(void) {
  ENTER_CRITICAL();
  ring_debug.w_ptr_tx = 0;
  ring_debug.r_ptr_tx = 0;
  ring_debug.w_ptr_rx = 0;
  ring_debug.r_ptr_rx = 0;
  EXIT_CRITICAL();
}

// ************************ High-level debug functions **********************
void putch(const char a) {
  // misra-c2012-17.7: serial debug function, ok to ignore output
  (void)injectc(a);
}

void puts(const char *a) {
  for (const char *in = a; *in; in++) {
    if (*in == '\n') putch('\r');
    putch(*in);
  }
}

void putui(uint32_t i) {
  uint32_t i_copy = i;
  char str[11];
  uint8_t idx = 10;
  str[idx] = '\0';
  idx--;
  do {
    str[idx] = (i_copy % 10U) + 0x30U;
    idx--;
    i_copy /= 10;
  } while (i_copy != 0U);
  puts(&str[idx + 1U]);
}

void puth(unsigned int i) {
  char c[] = "0123456789abcdef";
  for (int pos = 28; pos != -4; pos -= 4) {
    putch(c[(i >> (unsigned int)(pos)) & 0xFU]);
  }
}

void puth2(unsigned int i) {
  char c[] = "0123456789abcdef";
  for (int pos = 4; pos != -4; pos -= 4) {
    putch(c[(i >> (unsigned int)(pos)) & 0xFU]);
  }
}

void hexdump(const void *a, int l) {
  if (a != NULL) {
    for (int i=0; i < l; i++) {
      if ((i != 0) && ((i & 0xf) == 0)) puts("\n");
      puth2(((const unsigned char*)a)[i]);
      puts(" ");
    }
  }
  puts("\n");
}
