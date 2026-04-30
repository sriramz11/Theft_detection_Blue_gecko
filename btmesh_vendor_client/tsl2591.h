#ifndef TSL2591_H
#define TSL2591_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_i2cspm.h"

/* I2C Address (fixed) */
#define TSL2591_I2C_ADDR        0x29

/*
 * Command byte prefixes
 *   Bit 7 = 1  (command)
 *   Bit 6 = 0  (normal)
 *   Bit 5 = 0  -> single byte  (0xA0)
 *   Bit 5 = 1  -> auto-increment, required for 4-byte channel read (0xB0)
 */
#define TSL2591_CMD_BIT         0xA0    /* single-register access  */
#define TSL2591_CMD_BIT_AUTO    0xB0    /* auto-increment (multi-byte read) */

/* Registers */
#define TSL2591_REG_ENABLE      0x00
#define TSL2591_REG_CONTROL     0x01
#define TSL2591_REG_ID          0x12
#define TSL2591_REG_CH0_LOW     0x14
#define TSL2591_REG_CH0_HIGH    0x15
#define TSL2591_REG_CH1_LOW     0x16
#define TSL2591_REG_CH1_HIGH    0x17

/*
 * ENABLE register
 *   Bit 0: PON  - power on
 *   Bit 1: AEN  - ALS enable  (must be set or ADC never runs)
 */
#define TSL2591_POWER_ON        0x03    /* PON | AEN */

/*
 * CONTROL register  gain[5:4] | integration-time[2:0]
 * Low gain (1x), 100 ms integration - safe defaults
 */
#define TSL2591_CONTROL_INIT    0x00

/* Expected value of ID register */
#define TSL2591_ID_VAL          0x50

bool tsl2591_init(sl_i2cspm_t *i2c);
bool tsl2591_read_channels(sl_i2cspm_t *i2c,
                           uint16_t *ch0,
                           uint16_t *ch1);

#endif /* TSL2591_H */
