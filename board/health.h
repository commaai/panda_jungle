// When changing these structs, python/__init__.py needs to be kept up to date!

#define HEALTH_PACKET_VERSION 1
struct __attribute__((packed)) health_t {
  uint32_t uptime_pkt;
  float ch1_power;
  float ch2_power;
  float ch3_power;
  float ch4_power;
  float ch5_power;
  float ch6_power;
};

#define CAN_HEALTH_PACKET_VERSION 4
typedef struct __attribute__((packed)) {
  uint8_t bus_off;
  uint32_t bus_off_cnt;
  uint8_t error_warning;
  uint8_t error_passive;
  uint8_t last_error; // real time LEC value
  uint8_t last_stored_error; // last LEC positive error code stored
  uint8_t last_data_error; // DLEC (for CANFD only)
  uint8_t last_data_stored_error; // last DLEC positive error code stored (for CANFD only)
  uint8_t receive_error_cnt; // Actual state of the receive error counter, values between 0 and 127. FDCAN_ECR.REC
  uint8_t transmit_error_cnt; // Actual state of the transmit error counter, values between 0 and 255. FDCAN_ECR.TEC
  uint32_t total_error_cnt; // How many times any error interrupt was invoked
  uint32_t total_tx_lost_cnt; // Tx event FIFO element lost
  uint32_t total_rx_lost_cnt; // Rx FIFO 0 message lost due to FIFO full condition
  uint32_t total_tx_cnt;
  uint32_t total_rx_cnt;
  uint32_t total_fwd_cnt; // Messages forwarded from one bus to another
  uint32_t total_tx_checksum_error_cnt;
  uint16_t can_speed;
  uint16_t can_data_speed;
  uint8_t canfd_enabled;
  uint8_t brs_enabled;
  uint8_t canfd_non_iso;
} can_health_t;
