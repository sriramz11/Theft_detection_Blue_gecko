/***************************************************************************//**
 * @file tsl2591.c
 * @brief TSL2591 Ambient Light Sensor Driver
 *
 * @owner Prudhvi Raj Belide
 * @project ECEN 5823 - Backpack/Locker Anti-Tamper BT Mesh System
 * @node Low-Power Node (LPN), EFR32BG13 / Blue Gecko
 *
 * Fixes applied:
 *  FIX-A: tsl2591_write_reg called I2CSPM_Transfer twice — the first call's
 *          result was discarded and the transfer was repeated. Every write
 *          was being sent twice, corrupting ENABLE and CONTROL register state.
 *          Now called exactly once.
 *
 *  FIX-B: tsl2591_init had no delay after writing ENABLE (PON | AEN).
 *          The ADC integration engine needs at least one full integration
 *          period before CH0/CH1 data is valid. Without this delay the first
 *          read (if it happens quickly) returns 0 for both channels.
 *          A 120 ms delay (100 ms IT + 20 ms margin) is added after enabling.
 *
 * Design notes (do not change without reading):
 *  1. COMMAND BYTE: every register access requires TSL2591_CMD_BIT (0xA0)
 *     OR'd with the register address for single-byte access, or
 *     TSL2591_CMD_BIT_AUTO (0xB0) for auto-increment multi-byte reads.
 *     Using a bare register address silently fails with no NACK.
 *
 *  2. ENABLE register needs BOTH PON (bit 0) and AEN (bit 1) set.
 *     PON alone powers the oscillator but leaves ALS off; CH0/CH1 stay 0.
 *
 *  3. The 4-byte channel read uses 0xB0 (auto-increment) so the register
 *     pointer advances automatically across CH0_LOW/CH0_HIGH/CH1_LOW/CH1_HIGH.
 *     Using 0xA0 (no auto-increment) would return CH0_LOW four times.
 ******************************************************************************/

#include "tsl2591.h"
#include "sl_i2cspm.h"
#include "sl_sleeptimer.h"
#include "app_log.h"

/***************************************************************************//**
 * @brief Write one byte to a TSL2591 register.
 *        FIX-A: I2CSPM_Transfer called exactly once. Previous version called
 *        it twice and discarded the first result — every write was duplicated.
 ******************************************************************************/
static bool tsl2591_write_reg(sl_i2cspm_t *i2c, uint8_t reg, uint8_t value)
{
  I2C_TransferSeq_TypeDef    seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t buf[2];

  buf[0] = TSL2591_CMD_BIT | reg;   /* 0xA0 | reg — mandatory command byte */
  buf[1] = value;

  seq.addr        = TSL2591_I2C_ADDR << 1;
  seq.flags       = I2C_FLAG_WRITE;
  seq.buf[0].data = buf;
  seq.buf[0].len  = 2;

  ret = I2CSPM_Transfer(i2c, &seq);   /* FIX-A: called once, result kept */
  return (ret == i2cTransferDone);
}

/***************************************************************************//**
 * @brief Read one byte from a TSL2591 register.
 ******************************************************************************/
static bool tsl2591_read_reg(sl_i2cspm_t *i2c, uint8_t reg, uint8_t *out)
{
  I2C_TransferSeq_TypeDef    seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t cmd;

  cmd = TSL2591_CMD_BIT | reg;      /* 0xA0 | reg */

  seq.addr        = TSL2591_I2C_ADDR << 1;
  seq.flags       = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = &cmd;
  seq.buf[0].len  = 1;
  seq.buf[1].data = out;
  seq.buf[1].len  = 1;

  ret = I2CSPM_Transfer(i2c, &seq);
  return (ret == i2cTransferDone);
}

/***************************************************************************//**
 * @brief Initialize TSL2591.
 *
 * Steps:
 *  1. Read ID register to confirm device is on bus and ACKing.
 *  2. Write CONTROL to set gain and integration time.
 *  3. Write ENABLE with PON | AEN (both bits required).
 *  4. FIX-B: Wait one integration period + 20 ms margin before returning
 *     so the first read is guaranteed to return valid data.
 ******************************************************************************/
bool tsl2591_init(sl_i2cspm_t *i2c)
{
  uint8_t id = 0;

  /* Step 1 — verify device ID */
  if (!tsl2591_read_reg(i2c, TSL2591_REG_ID, &id)) {
    app_log("[TSL2591] ERROR: ID read failed (check wiring / pull-ups)\r\n");
    return false;
  }
  if (id != TSL2591_ID_VAL) {
    app_log("[TSL2591] ERROR: wrong ID 0x%02X (expected 0x%02X)\r\n",
            id, TSL2591_ID_VAL);
    return false;
  }
  app_log("[TSL2591] ID = 0x%02X  OK\r\n", id);

  /* Step 2 — set gain (1x) and integration time (100 ms) */
  if (!tsl2591_write_reg(i2c, TSL2591_REG_CONTROL, TSL2591_CONTROL_INIT)) {
    app_log("[TSL2591] ERROR: CONTROL write failed\r\n");
    return false;
  }

  /* Step 3 — enable: power on oscillator + start ALS */
  if (!tsl2591_write_reg(i2c, TSL2591_REG_ENABLE, TSL2591_POWER_ON)) {
    app_log("[TSL2591] ERROR: ENABLE write failed\r\n");
    return false;
  }

  /*
   * FIX-B: Wait one full integration period + 20 ms guard margin.
   * TSL2591_CONTROL_INIT selects 100 ms integration, so we wait 120 ms.
   * Without this, the very first tsl2591_read_channels() call after init
   * may return 0 for both channels.
   */
  sl_sleeptimer_delay_millisecond(120);

  app_log("[TSL2591] Init OK\r\n");
  return true;
}

/***************************************************************************//**
 * @brief Read both ADC channels.
 *
 * Uses TSL2591_CMD_BIT_AUTO (0xB0) so the I2C register pointer auto-increments
 * across all four channel bytes in a single transaction:
 *   data[0] = CH0_LOW   data[1] = CH0_HIGH
 *   data[2] = CH1_LOW   data[3] = CH1_HIGH
 *
 * Using 0xA0 (no auto-increment) would read CH0_LOW four times.
 ******************************************************************************/
bool tsl2591_read_channels(sl_i2cspm_t *i2c,
                           uint16_t    *ch0,
                           uint16_t    *ch1)
{
  I2C_TransferSeq_TypeDef    seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t cmd;
  uint8_t data[4];

  cmd = TSL2591_CMD_BIT_AUTO | TSL2591_REG_CH0_LOW;   /* 0xB0 | 0x14 */

  seq.addr        = TSL2591_I2C_ADDR << 1;
  seq.flags       = I2C_FLAG_WRITE_READ;
  seq.buf[0].data = &cmd;
  seq.buf[0].len  = 1;
  seq.buf[1].data = data;
  seq.buf[1].len  = 4;

  ret = I2CSPM_Transfer(i2c, &seq);
  if (ret != i2cTransferDone) {
    app_log("[TSL2591] ERROR: channel read failed err=%d\r\n", ret);
    return false;
  }

  *ch0 = ((uint16_t)data[1] << 8) | data[0];
  *ch1 = ((uint16_t)data[3] << 8) | data[2];

  app_log("[TSL2591] CH0=%5u  CH1=%5u\r\n", *ch0, *ch1);
  return true;
}
