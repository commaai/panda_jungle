// ******************** Prototypes ********************
typedef void (*board_init)(void);
typedef void (*board_set_led)(uint8_t color, bool enabled);
typedef void (*board_board_tick)(void);

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

// CAN modes
#define CAN_MODE_NORMAL 0U
#define CAN_MODE_GMLAN_CAN2 1U
#define CAN_MODE_GMLAN_CAN3 2U
#define CAN_MODE_OBD_CAN2 3U

// Harness states
#define HARNESS_ORIENTATION_NONE 0U
#define HARNESS_ORIENTATION_1 1U
#define HARNESS_ORIENTATION_2 2U

// ********************* Globals **********************
uint8_t harness_orientation = HARNESS_ORIENTATION_NONE;
uint8_t can_mode = CAN_MODE_NORMAL;
bool ignition = false;
