/***************************************************************************//**
 * @file vcnl4010.c
 * @brief VCNL4010 Proximity Sensor Driver - Implementation
 *
 * @owner Prudhvi Raj Belide
 * @project ECEN 5823 - Backpack/Locker Anti-Tamper BT Mesh System
 * @node Low-Power Node (LPN)
 *
 * Milestone 1: Initialize sensor, read raw proximity count, print to terminal.
 *
 * The VCNL4010 uses a standard I2C register map. There is no command-bit
 * requirement like the TSL2561 - registers are accessed directly by address.
 *
 * Reading strategy used here: on-demand (one-shot) proximity trigger.
 * This is friendlier for a low-power node than continuous self-timed mode
 * because we only draw IR LED current when we actually need a reading.
 ******************************************************************************/

#include "vcnl4010.h"
#include "app_log.h"
#include "sl_sleeptimer.h"
#include <stdbool.h>
#include "em_i2c.h"

// ---------------------------------------------------------------------------
// Private: maximum poll iterations before we declare a timeout
// At 402ms integration it should be ready in < 5ms for proximity
// ---------------------------------------------------------------------------
#define VCNL4010_PROX_READY_TIMEOUT_LOOPS  (5000)

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

/**
 * @brief Write one byte to a VCNL4010 register.
 *        I2C format: [START][ADDR W][REG][DATA][STOP]
 */
static bool vcnl4010_write_reg(sl_i2cspm_t *i2cspm,
                                uint8_t      reg,
                                uint8_t      data)
{
    I2C_TransferSeq_TypeDef seq;
    uint8_t write_buf[2];

    write_buf[0] = reg;
    write_buf[1] = data;

    seq.addr        = VCNL4010_I2C_ADDR << 1;
    seq.flags       = I2C_FLAG_WRITE;
    seq.buf[0].data = write_buf;
    seq.buf[0].len  = 2;

    I2C_TransferReturn_TypeDef result = I2CSPM_Transfer(i2cspm, &seq);
    if (result != i2cTransferDone) {
        app_log("VCNL4010: write_reg 0x%02X failed, err=%d\r\n", reg, (int)result);
        return false;
    }
    return true;
}

/**
 * @brief Read one byte from a VCNL4010 register.
 *        I2C format: [START][ADDR W][REG][RESTART][ADDR R][DATA][STOP]
 */
static bool vcnl4010_read_reg(sl_i2cspm_t *i2cspm,
                               uint8_t      reg,
                               uint8_t     *out)
{
    I2C_TransferSeq_TypeDef seq;
    uint8_t write_buf[1];

    write_buf[0] = reg;

    seq.addr        = VCNL4010_I2C_ADDR << 1;
    seq.flags       = I2C_FLAG_WRITE_READ;
    seq.buf[0].data = write_buf;
    seq.buf[0].len  = 1;
    seq.buf[1].data = out;
    seq.buf[1].len  = 1;

    I2C_TransferReturn_TypeDef result = I2CSPM_Transfer(i2cspm, &seq);
    if (result != i2cTransferDone) {
        app_log("VCNL4010: read_reg 0x%02X failed, err=%d\r\n", reg, (int)result);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

bool vcnl4010_init(sl_i2cspm_t *i2cspm)
{
    uint8_t product_id = 0;

    // 1. Read product ID register to verify we are talking to a VCNL4010
    if (!vcnl4010_read_reg(i2cspm, VCNL4010_REG_PRODUCT_ID, &product_id)) {
        app_log("VCNL4010: Product ID read failed\r\n");
        return false;
    }

    if (product_id != VCNL4010_PRODUCT_ID_EXPECTED) {
        app_log("VCNL4010: Unexpected product ID 0x%02X (expected 0x%02X)\r\n",
                product_id, VCNL4010_PRODUCT_ID_EXPECTED);
        // Note: Do NOT return false here during bringup.
        // Some breakout boards use slightly different rev nibbles.
        // Log the warning and continue; change to 'return false' once confirmed.
    } else {
        app_log("VCNL4010: Init OK  Product_ID=0x%02X\r\n", product_id);
    }

    // 2. Set IR LED current to 200 mA for good proximity range
    //    (reduce to VCNL4010_IR_LED_100MA if you see saturation)
    if (!vcnl4010_write_reg(i2cspm, VCNL4010_REG_IR_LED_CURRENT, VCNL4010_IR_LED_100MA)) {
        app_log("VCNL4010: Set IR LED current failed\r\n");
        return false;
    }

    // 3. Set proximity measurement rate (only matters for self-timed mode)
    if (!vcnl4010_write_reg(i2cspm, VCNL4010_REG_PROX_RATE, VCNL4010_PROX_RATE_7_81)) {
        app_log("VCNL4010: Set prox rate failed\r\n");
        return false;
    }

    // We do NOT enable self-timed mode here. We use on-demand reads in
    // vcnl4010_read_proximity() to stay power-friendly on the LPN.

    return true;
}

bool vcnl4010_read_proximity(sl_i2cspm_t *i2cspm, uint16_t *prox_out)
{
    uint8_t cmd      = 0;
    uint8_t high     = 0;
    uint8_t low      = 0;
    int     timeout  = VCNL4010_PROX_READY_TIMEOUT_LOOPS;

    // 1. Trigger a single on-demand proximity measurement
    //    Read current command register, then set the PROX_OD bit
    if (!vcnl4010_read_reg(i2cspm, VCNL4010_REG_COMMAND, &cmd)) return false;
    cmd |= VCNL4010_CMD_PROX_OD;
    if (!vcnl4010_write_reg(i2cspm, VCNL4010_REG_COMMAND, cmd)) return false;

    // 2. Poll PROX_RDY bit until conversion is complete
    do {
        if (!vcnl4010_read_reg(i2cspm, VCNL4010_REG_COMMAND, &cmd)) return false;
        timeout--;
    } while (!(cmd & VCNL4010_CMD_PROX_RDY) && (timeout > 0));

    if (timeout <= 0) {
        app_log("VCNL4010: Proximity ready timeout\r\n");
        return false;
    }

    // 3. Read the 16-bit result (high byte first)
    if (!vcnl4010_read_reg(i2cspm, VCNL4010_REG_PROX_HI, &high)) return false;
    if (!vcnl4010_read_reg(i2cspm, VCNL4010_REG_PROX_LO, &low))  return false;

    *prox_out = (uint16_t)((high << 8) | low);
    return true;
}
