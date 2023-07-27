#define SDMMC_TIMEOUT 1000000U


bool sdmmc_send_command(uint32_t cmd_index, uint32_t cmd_arg, uint32_t wait_resp) {
  // Clear command flags
  SDMMC1->ICR = SDMMC_ICR_CMDRENDC |
                SDMMC_ICR_CMDSENTC |
                SDMMC_ICR_DATAENDC |
                SDMMC_ICR_DBCKENDC |
                SDMMC_ICR_SDIOITC;

  // Configure the command
  SDMMC1->ARG = cmd_arg;
  SDMMC1->CMD = (cmd_index << SDMMC_CMD_CMDINDEX_Pos) |
                (wait_resp << SDMMC_CMD_WAITRESP_Pos) |
                SDMMC_CMD_CPSMEN;

  SDMMC1->ACKTIME = 0x0FFFFFFF;

  // Wait for command response
  uint32_t timeout = SDMMC_TIMEOUT;
  while ((SDMMC1->STA & SDMMC_STA_CMDREND) == 0) {
    if (timeout == 0) {
      print("sdmmc_send_command: timeout\n");
      return false;
    }
    timeout--;
  }

  // Check for command errors
  if ((SDMMC1->STA & SDMMC_STA_CMDSENT) == 0) {
    print("sdmmc_send_command: command error\n");
    return false;
  }

  return true;
}

// Read blocks of data from the SD card
bool sdmmc_read_blocks(uint32_t block_addr, uint32_t num_blocks, uint8_t *data) {
  // Configure the data transfer
  SDMMC1->DTIMER = SDMMC_TIMEOUT;
  SDMMC1->DLEN = num_blocks * 512;
  SDMMC1->DCTRL = SDMMC_DCTRL_DTEN | SDMMC_DCTRL_DTDIR;

  // Send the read command
  if (!sdmmc_send_command(17, block_addr, 1)) {
    print("sdmmc_read_blocks: send_command failed\n");
    return false;
  }

  // Wait for data end
  uint32_t timeout = SDMMC_TIMEOUT;
  while ((SDMMC1->STA & SDMMC_STA_DATAEND) == 0) {
    if (timeout == 0) {
      print("sdmmc_read_blocks: timeout\n");
      return false;
    }
    timeout--;
  }

  // Read the data
  for (uint32_t i = 0; i < num_blocks * 512; i += 4) {
    *(uint32_t *)(data + i) = SDMMC1->FIFO;
  }

  return true;
}

// Write blocks of data to the SD card
bool sdmmc_write_blocks(uint32_t block_addr, uint32_t num_blocks, const uint8_t *data) {
  // Configure the data transfer
  SDMMC1->DTIMER = SDMMC_TIMEOUT;
  SDMMC1->DLEN = num_blocks * 512;
  SDMMC1->DCTRL = SDMMC_DCTRL_DTEN;

  // Send the write command
  if (!sdmmc_send_command(24, block_addr, 1)) {
    return false;
  }

  // Wait for data end
  uint32_t timeout = SDMMC_TIMEOUT;
  while ((SDMMC1->STA & SDMMC_STA_DATAEND) == 0) {
    if (timeout == 0) {
      return false;
    }
    timeout--;
  }

  // Write the data
  for (uint32_t i = 0; i < num_blocks * 512; i += 4) {
    SDMMC1->FIFO = *(uint32_t *)(data + i);
  }

  // Wait for data to be written
  timeout = SDMMC_TIMEOUT;
  while ((SDMMC1->STA & SDMMC_STA_DBCKEND) == 0) {
    if (timeout == 0) {
      return false;
    }
    timeout--;
  }

  return true;
}

void sdmmc_init(void) {
  // Disable SDMMC1 peripheral
  SDMMC1->POWER = 0;

  // Configure SDMMC1 clock
  SDMMC1->CLKCR = (0xFF << SDMMC_CLKCR_CLKDIV_Pos) | SDMMC_CLKCR_PWRSAV | SDMMC_CLKCR_NEGEDGE;

  // Enable SDMMC1 peripheral
  SDMMC1->POWER |= SDMMC_POWER_PWRCTRL;

  // Wait until SDMMC1 is ready
  while ((SDMMC1->POWER & SDMMC_POWER_PWRCTRL) == 0) {}

  // Set the data bus width to 4 bits
  SDMMC1->CLKCR |= SDMMC_CLKCR_WIDBUS_0;

  // Set the block size to 512 bytes
  SDMMC1->DCTRL |= (0x9 << SDMMC_DCTRL_DBLOCKSIZE_Pos);

  // Enable data transfer in SDMMC1 peripheral
  // SDMMC1->DCTRL = SDMMC_DCTRL_DTEN;

  // Send CMD0 to reset the SD card
  sdmmc_send_command(0, 0, 0);

  delay(100000);

  // Send CMD8 to check the SD card voltage range and supply voltage levels
  sdmmc_send_command(8, 0x1AA, 1);

  // Send CMD55 and ACMD41 to initialize the SD card
  // uint32_t resp;
  // uint32_t timeout = 20;
  // do {
  //   // Send CMD55 to indicate the next command is an application specific command
  //   sdmmc_send_command(55, 0, 1);

  //   // Send ACMD41 with HCS (High Capacity Support) bit set to 1 to initialize the SD card
  //   sdmmc_send_command(41, 0x40000000, 1);

  //   // Read the response
  //   resp = SDMMC1->RESP1;

  //   if (timeout-- == 0) {
  //     print("sdmmc_init: timeout\n");
  //     return;
  //   }
  // } while ((resp & 0x80000000) == 0);  // Check if the initialization is complete (bit 31 set to 1)

  // // Send CMD2 to get the card identification register (CID)
  // sdmmc_send_command(2, 0, 1);

  // // Send CMD3 to get the relative card address (RCA)
  // sdmmc_send_command(3, 0, 1);

  // // Send CMD9 to get the card specific data (CSD)
  // sdmmc_send_command(9, 0, 1);


}