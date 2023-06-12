// ******************** Prototypes ********************
typedef void (*board_init)(void);
typedef void (*board_set_led)(uint8_t color, bool enabled);
typedef void (*board_board_tick(void));

struct board {
  const char *board_type;
  const bool has_canfd;
  const uint16_t avdd_mV;
  board_init init;
  board_set_led set_led;
  board_board_tick board_tick;
};

// ******************* Definitions ********************
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_V1 1U
#define HW_TYPE_V2 2U

// LED colors
#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U

