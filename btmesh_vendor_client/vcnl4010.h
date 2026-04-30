/***************************************************************************//**
 * @file vcnl4010.h
 * @brief VCNL4010 Proximity Sensor Driver - Header
 *
 * @owner Prudhvi Raj Belide
 * @project ECEN 5823 - Backpack/Locker Anti-Tamper BT Mesh System
 * @node Low-Power Node (LPN)
 *
 * Milestone 1: Initialize sensor, read raw proximity count, print to terminal.
 *
 * Datasheet: https://www.vishay.com/docs/83462/vcnl4010.pdf
 *******************************************************************************
 * VCNL4010 fixed I2C address: 0x13 (not configurable)
 ******************************************************************************/

#ifndef VCNL4010_H_
#define VCNL4010_H_

#include <stdint.h>
#include <stdbool.h>
#include "sl_i2cspm.h"

// ---------------------------------------------------------------------------
// I2C Address (fixed, cannot be changed)
// ---------------------------------------------------------------------------
#define VCNL4010_I2C_ADDR           (0x13)

// ---------------------------------------------------------------------------
// Register Addresses
// ---------------------------------------------------------------------------
#define VCNL4010_REG_COMMAND        (0x80)  // Command register
#define VCNL4010_REG_PRODUCT_ID     (0x81)  // Product ID / Revision
#define VCNL4010_REG_PROX_RATE      (0x82)  // Proximity measurement rate
#define VCNL4010_REG_IR_LED_CURRENT (0x83)  // IR LED current setting
#define VCNL4010_REG_AMBIENT_PARAM  (0x84)  // Ambient light parameter
#define VCNL4010_REG_AMBIENT_HI     (0x85)  // Ambient light result high byte
#define VCNL4010_REG_AMBIENT_LO     (0x86)  // Ambient light result low byte
#define VCNL4010_REG_PROX_HI        (0x87)  // Proximity result high byte
#define VCNL4010_REG_PROX_LO        (0x88)  // Proximity result low byte
#define VCNL4010_REG_INT_CTRL       (0x89)  // Interrupt control
#define VCNL4010_REG_LOW_THR_HI     (0x8A)  // Low threshold high byte
#define VCNL4010_REG_LOW_THR_LO     (0x8B)  // Low threshold low byte
#define VCNL4010_REG_HIGH_THR_HI    (0x8C)  // High threshold high byte
#define VCNL4010_REG_HIGH_THR_LO    (0x8D)  // High threshold low byte
#define VCNL4010_REG_INT_STATUS     (0x8E)  // Interrupt status
#define VCNL4010_REG_PROX_MOD       (0x8F)  // Proximity modulator timing

// Command register bit masks
#define VCNL4010_CMD_SELFTIMED_EN   (1 << 0)  // Enable self-timed measurements
#define VCNL4010_CMD_PROX_EN        (1 << 1)  // Enable proximity measurement
#define VCNL4010_CMD_ALS_EN         (1 << 2)  // Enable ambient light measurement
#define VCNL4010_CMD_PROX_OD        (1 << 3)  // Trigger one on-demand proximity
#define VCNL4010_CMD_ALS_OD         (1 << 4)  // Trigger one on-demand ambient
#define VCNL4010_CMD_PROX_RDY       (1 << 5)  // Proximity data ready flag
#define VCNL4010_CMD_ALS_RDY        (1 << 6)  // Ambient data ready flag

// IR LED current: value in steps of 10 mA, 0 = 0 mA ... 20 = 200 mA
// 20 = 200 mA is maximum; 10 = 100 mA is a safe default
#define VCNL4010_IR_LED_200MA       (20)
#define VCNL4010_IR_LED_100MA       (10)

// Proximity measurement rate: measurements per second
// 0=1.95, 1=3.90, 2=7.81, 3=16.62, 4=31.25, 5=62.5, 6=125, 7=250 meas/s
#define VCNL4010_PROX_RATE_7_81     (2)   // ~8 measurements/second default

// Expected Product ID register value (upper nibble = product, lower = rev)
#define VCNL4010_PRODUCT_ID_EXPECTED (0x21)

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief  Initialize the VCNL4010 proximity sensor.
 *         Verifies device ID, sets IR LED current, enables self-timed
 *         proximity measurements.
 * @param  i2cspm  Pointer to an initialized I2CSPM instance.
 * @return true on success, false on I2C error or device not found.
 */
bool vcnl4010_init(sl_i2cspm_t *i2cspm);

/**
 * @brief  Read a single proximity measurement (on-demand, blocking).
 *         Triggers one measurement, polls the ready bit, then returns result.
 *         Suitable for low-power periodic wakeup style of reading.
 * @param  i2cspm   Pointer to I2CSPM instance.
 * @param  prox_out Output: raw 16-bit proximity count.
 *                  Higher count = closer object.
 * @return true on success, false on I2C error or timeout.
 */
bool vcnl4010_read_proximity(sl_i2cspm_t *i2cspm, uint16_t *prox_out);

#endif /* VCNL4010_H_ */
