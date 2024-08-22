#include <stdint.h>
#include <string.h>
#include "debug_cm.h"

// AP CSW register, base value
#define CSW_VALUE (CSW_RESERVED | CSW_MSTRDBG | CSW_HPROT | CSW_DBGSTAT | CSW_SADDRINC)

#define MAX_SWD_RETRY 10

extern uint8_t SWD_Transfer(uint8_t req, __xdata uint8_t *data);

static void int2array(__xdata uint8_t *res, uint32_t data, uint8_t len) {
  uint8_t i = 0;

  for(i = 0; i < len; i++) {
    res[i] = (data >> 8 * (i)) & 0xFF;
  }
}

static uint8_t swd_transfer_retry(uint8_t req, __xdata uint8_t *data) {
  uint8_t i, ack;

  for (i = 0; i < MAX_SWD_RETRY; i++) {
    ack = SWD_Transfer(req, data);

    if (ack != DAP_TRANSFER_WAIT) {
      return ack;
    }
  }

  return ack;
}

// Write debug port register
uint8_t swd_write_dp(uint8_t adr, uint32_t val) {
  uint8_t req;
  __xdata uint8_t data[4];
  uint8_t ack;

  req = SWD_REG_DP | SWD_REG_W | SWD_REG_ADR(adr);
  int2array(data, val, 4);
  ack = swd_transfer_retry(req, data);
  return (ack == 0x01);
}

// Write access port register
uint8_t swd_write_ap(uint32_t adr, uint32_t val) {
  __xdata uint8_t data[4];
  uint8_t req, ack;
  uint32_t apsel = adr & 0xff000000;
  uint32_t bank_sel = adr & APBANKSEL;

  if (!swd_write_dp(DP_SELECT, apsel | bank_sel)) {
    return 0;
  }

  req = SWD_REG_AP | SWD_REG_W | SWD_REG_ADR(adr);
  int2array(data, val, 4);
  if (swd_transfer_retry(req, data) != 0x01) {
    return 0;
  }

  req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
  ack = swd_transfer_retry(req, NULL);
  return (ack == 0x01);
}

// Write target memory.
static uint8_t swd_write_data(uint32_t address, uint32_t data) {
  __xdata uint8_t tmp_in[4];
  uint8_t req, ack;
  // put addr in TAR register
  int2array(tmp_in, address, 4);
  req = SWD_REG_AP | SWD_REG_W | (1 << 2);
  if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01) {
    return 0;
  }

  // write data
  int2array(tmp_in, data, 4);
  req = SWD_REG_AP | SWD_REG_W | (3 << 2);
  if (swd_transfer_retry(req, (uint32_t *)tmp_in) != 0x01) {
    return 0;
  }

  // dummy read
  req = SWD_REG_DP | SWD_REG_R | SWD_REG_ADR(DP_RDBUFF);
  ack = swd_transfer_retry(req, NULL);
  return (ack == 0x01) ? 1 : 0;
}

uint8_t swd_write_word(uint32_t addr, uint32_t val) {
  if (!swd_write_ap(AP_CSW, CSW_VALUE | CSW_SIZE32)) {
    return 0;
  }

  if (!swd_write_data(addr, val)) {
    return 0;
  }

  return 1;
}
