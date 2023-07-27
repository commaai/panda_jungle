// ******************** Prototypes ********************
void print(const char *a);
void puth(unsigned int i);
void puth2(unsigned int i);
void puth4(unsigned int i);
void hexdump(const void *a, int l);
typedef struct board board;
typedef struct harness_configuration harness_configuration;

// ********************* Globals **********************
uint8_t hw_type = 0;
const board *current_board;
uint32_t uptime_cnt = 0;
bool green_led_enabled = false;
