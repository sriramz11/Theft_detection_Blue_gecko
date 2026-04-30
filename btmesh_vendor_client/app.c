/***************************************************************************//**
 * @file app.c
 * @brief Core application logic for the vendor client node.
 * @owner Prudhvi Belide
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

// DOS: For ECEN 5823 this is the code for a BT Mesh Client/LPN.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tsl2591.h"
#include "sl_i2cspm_sensor_config.h"

#include "em_common.h"
#include "app_assert.h"
#include "app_log.h"
#include "sl_status.h"
#include "app.h"

#include "sl_btmesh_api.h"
#include "sl_bt_api.h"
#include "sl_simple_timer.h"
#include "sl_sleeptimer.h"

#include "em_cmu.h"
#include "em_gpio.h"
#include "em_rtcc.h"

#include "my_model_def.h"

#include "app_button_press.h"
#include "sl_simple_button.h"
#include "sl_simple_button_instances.h"

#include "sl_btmesh_wstk_lcd.h"

// Milestone 1 sensor files
#include "vcnl4010.h"
#include "event_logic.h"
#include "sl_i2cspm_instances.h"

#include "em_gpio.h"

#include "em_i2c.h"

// DOS added this #define to control counting of
//     sl_btmesh_evt_lpn_friendship_terminated_id events
//#define COUNT_FRIENDSHIP_TERMINATED_EVENTS (1)

#ifdef PROV_LOCALLY
// Group Addresses
// Choose any 16-bit address starting at 0xC000
#define CUSTOM_STATUS_GRP_ADDR                      0xC001
#define CUSTOM_CTRL_GRP_ADDR                        0xC002

// The default settings of the network and the node
#define NET_KEY_IDX                                 0
#define APP_KEY_IDX                                 0
#define IVI                                         0
#define DEFAULT_TTL                                 5
#endif

#define EX_B0_PRESS                                 ((1) << 5)
#define EX_B0_LONG_PRESS                            ((1) << 6)
#define EX_B1_PRESS                                 ((1) << 7)
#define EX_B1_LONG_PRESS                            ((1) << 8)

// Timing
#define STEP_RES_100_MILLI                          0
#define STEP_RES_1_SEC                              ((1) << 6)
#define STEP_RES_10_SEC                             ((2) << 6)
#define STEP_RES_10_MIN                             ((3) << 6)

#define STEP_RES_BIT_MASK                           0xC0

#define SET_100_MILLI(x)                            (uint8_t)(STEP_RES_100_MILLI | ((x) & (0x3F)))
#define SET_1_SEC(x)                                (uint8_t)(STEP_RES_1_SEC | ((x) & (0x3F)))
#define SET_10_SEC(x)                               (uint8_t)(STEP_RES_10_SEC | ((x) & (0x3F)))
#define SET_10_MIN(x)                               (uint8_t)(STEP_RES_10_MIN | ((x) & (0x3F)))

#define PB_ADV                                      0x1
#define PB_GATT                                     0x2

#define BUTTON_PRESS_BUTTON_0                       0
#define BUTTON_PRESS_BUTTON_1                       1

uint8_t conn_handle = 0xFF;

static alarm_state_t last_state = STATE_NONE;

static uint32_t periodic_timer_ms = 0;
static uint8_t update_interval = 0;
static unit_t unit = celsius;

static uint8_t period_idx = 0;

static uint8_t periods[] = {
  SET_1_SEC(2),
  0,
  SET_1_SEC(5),
  0,
  SET_1_SEC(10),
  0,
  SET_10_SEC(12),
  0,
  SET_10_MIN(1),
  0
};

my_model_t my_model = {
  .elem_index = PRIMARY_ELEMENT,
  .vendor_id = MY_VENDOR_ID,
  .model_id = MY_MODEL_CLIENT_ID,
  .publish = 1,
  .opcodes_len = NUMBER_OF_OPCODES,
  .opcodes_data[0] = temperature_get,
  .opcodes_data[1] = temperature_status,
  .opcodes_data[2] = unit_get,
  .opcodes_data[3] = unit_set,
  .opcodes_data[4] = unit_set_unack,
  .opcodes_data[5] = unit_status,
  .opcodes_data[6] = update_interval_get,
  .opcodes_data[7] = update_interval_set,
  .opcodes_data[8] = update_interval_set_unack,
  .opcodes_data[9] = update_interval_status,
  .opcodes_data[10]  = alarm_safe,
  .opcodes_data[11]  = alarm_open,
  .opcodes_data[12]  = alarm_tamper,
  .opcodes_data[13]  = lux_status
};

#ifdef PROV_LOCALLY
static uint16_t uni_addr = 0;
static aes_key_128 enc_key = {
  .data = "\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03\x03"
};
#endif

bd_addr myAddress;
uint8_t myAddressType;

static void factory_reset(void);
static void delay_reset_ms(uint32_t ms);
static void parse_period(uint8_t interval);

// DOS global
uint32_t logging_timestamp = 0;

// Milestone 1 sensor poll timer
static sl_simple_timer_t sensor_poll_timer;
static volatile bool sensor_read_pending = false;

/**************************************************************************//**
 * Sensor poll callback
 *****************************************************************************/
static void sensor_poll_timer_cb(sl_simple_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sensor_read_pending = true;   // flag only — NO I2C here
}

/**************************************************************************//**
 * Logging timer callback
 *****************************************************************************/
static void logging_timer_cb(sl_simple_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;

  logging_timestamp += 500;
}

/**************************************************************************//**
 * Public function to retrieve the timestamp
 *****************************************************************************/
uint32_t get_logger_timestamp(void)
{
  return logging_timestamp;
}

//Temp
void i2c_scan(sl_i2cspm_t *i2c)
{
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;
  uint8_t dummy = 0;

  app_log("I2C scan:\r\n");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    seq.addr        = addr << 1;
    seq.flags       = I2C_FLAG_WRITE;
    seq.buf[0].data = &dummy;
    seq.buf[0].len  = 1;
    ret = I2CSPM_Transfer(i2c, &seq);
    if (ret == i2cTransferDone) {
      app_log("  Found: 0x%02X\r\n", addr);
    } else if (ret == i2cTransferNack) {
      // normal - no device at this address
    } else {
      app_log("  Bus error at 0x%02X, err=%d - stopping\r\n", addr, ret);
      break;
    }
  }
  app_log("I2C scan done\r\n");
}
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  app_log("=================\r\n");
  app_log("Client/LPN\r\n");

  app_button_press_enable();

  GPIO_PinModeSet(Si7021SENSOR_EN_port, Si7021SENSOR_EN_pin, gpioModePushPull, false);
  GPIO_PinOutSet(Si7021SENSOR_EN_port, Si7021SENSOR_EN_pin);

  sl_sleeptimer_delay_millisecond(500);

  char device_type[LCD_ROW_LEN];
  memset(device_type, 0, LCD_ROW_LEN);
  snprintf(device_type, LCD_ROW_LEN, "Client/LPN");
  sl_btmesh_LCD_write(device_type, LCD_ROW_1);

  static sl_simple_timer_t logging_timer;
  sl_simple_timer_start(&logging_timer,
                        500,
                        logging_timer_cb,
                        NULL,
                        true);

        // Milestone 1 sensor init
          app_log("\r\n--- SENSOR INIT ---\r\n");
            if (!tsl2591_init(sl_i2cspm_sensor)) {
                  app_log("  TSL2591  : FAILED\r\n");
              } else {
                  app_log("  TSL2591  : OK\r\n");
              }
            if (!vcnl4010_init(sl_i2cspm_sensor)) {
                app_log("  VCNL4010 : FAILED\r\n");
            } else {
                app_log("  VCNL4010 : OK\r\n");
            }
            app_log("  Poll timer : 3s\r\n");
            app_log("-------------------\r\n\r\n");

            sl_simple_timer_start(&sensor_poll_timer,
                        3000,
                        sensor_poll_timer_cb,
                        NULL,
                        true);

            app_log("Sensor poll timer started (3s interval).\r\n");
}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  if (!sensor_read_pending) return;
  sensor_read_pending = false;

  uint16_t light_ch0 = 0, light_ch1 = 0, proximity = 0;

  bool light_ok = tsl2591_read_channels(sl_i2cspm_sensor, &light_ch0, &light_ch1);
  bool prox_ok  = vcnl4010_read_proximity(sl_i2cspm_sensor, &proximity);

  if (light_ok) {
    app_log("[SENSOR] TSL2591  CH0=%5u  CH1=%5u\r\n", light_ch0, light_ch1);
  } else {
    app_log("[SENSOR] TSL2591  READ ERROR\r\n");
  }

  if (prox_ok) {
    app_log("[SENSOR] VCNL4010 PROX=%5u\r\n", proximity);
  } else {
    app_log("[SENSOR] VCNL4010 READ ERROR\r\n");
  }

  if (!light_ok || !prox_ok) return;

  alarm_state_t state = event_logic_evaluate(light_ch0, proximity);

  app_log("[STATE]  %-8s  light=%u/%u  prox=%u/%u\r\n",
          event_logic_state_str(state),
          light_ch0, (uint16_t)LIGHT_THRESHOLD,
          proximity,  (uint16_t)PROX_THRESHOLD);

  // Add these:
  char lcd_line1[LCD_ROW_LEN];
  char lcd_line2[LCD_ROW_LEN];

  snprintf(lcd_line1, LCD_ROW_LEN, "LUX:%u", light_ch0);
  snprintf(lcd_line2, LCD_ROW_LEN, "%s", event_logic_state_str(state));

  sl_btmesh_LCD_write(lcd_line1, LCD_ROW_2);
  sl_btmesh_LCD_write(lcd_line2, LCD_ROW_3);


//  if (state == last_state) return;
//  last_state = state;

  uint8_t opcode;
  uint8_t payload = (uint8_t)state;
  switch (state) {
    case STATE_SAFE:   opcode = 0x10; break;
    case STATE_OPEN:   opcode = 0x11; break;
    case STATE_TAMPER: opcode = 0x12; break;
    default: return;
  }

  sl_status_t sc;
  sc = sl_btmesh_vendor_model_set_publication(
         my_model.elem_index, my_model.vendor_id, my_model.model_id,
         opcode, 1, 1, &payload);
  if (sc != SL_STATUS_OK) {
    app_log("[MESH]  set_publication failed: 0x%04lX\r\n", (unsigned long)sc);
    return;
  }

  sc = sl_btmesh_vendor_model_publish(
         my_model.elem_index, my_model.vendor_id, my_model.model_id);
  if (sc != SL_STATUS_OK) {
    app_log("[MESH]  publish failed: 0x%04lX\r\n", (unsigned long)sc);
  } else {
    app_log("[MESH]  published opcode=0x%02X  state=%s\r\n",
            opcode, event_logic_state_str(state));
    // Publish LUX value
     uint16_t lux_payload = light_ch0;
     sc = sl_btmesh_vendor_model_set_publication(
            my_model.elem_index, my_model.vendor_id, my_model.model_id,
            lux_status, 1, sizeof(lux_payload), (uint8_t *)&lux_payload);
     if (sc == SL_STATUS_OK) {
       sl_btmesh_vendor_model_publish(
         my_model.elem_index, my_model.vendor_id, my_model.model_id);
       app_log("[MESH]  published lux=%u\r\n", light_ch0);
     }
  }
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 *****************************************************************************/
void sl_bt_on_event(struct sl_bt_msg *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {

    case sl_bt_evt_system_boot_id:
      if (GPIO_PinInGet(PB0_port, PB0_pin) == 0) {
        while (GPIO_PinInGet(PB0_port, PB0_pin) == 0) {
          ;
        }
        factory_reset();
      }

      app_log("Node init\r\n");
      sc = sl_btmesh_node_init();
      app_assert_status_f(sc, "Failed to init node\r\n");

      sc = sl_bt_system_get_identity_address(&myAddress, &myAddressType);
      if (sc != SL_STATUS_OK) {
        app_log("sl_bt_system_get_identity_address() returned != 0 status=0x%04x",
                (unsigned int)sc);
      } else {
        char myAddressStr[LCD_ROW_LEN];
        memset(myAddressStr, 0, LCD_ROW_LEN);
        snprintf(myAddressStr,
                 LCD_ROW_LEN,
                 "%02x:%02x:%02x:%02x:%02x:%02x",
                 myAddress.addr[5],
                 myAddress.addr[4],
                 myAddress.addr[3],
                 myAddress.addr[2],
                 myAddress.addr[1],
                 myAddress.addr[0]);
        sl_btmesh_LCD_write(myAddressStr, LCD_ROW_4);
      }

      break;

    case sl_bt_evt_system_external_signal_id: {
      uint8_t opcode = 0;
      uint8_t length = 0;
      uint8_t data = 0;

      if (evt->data.evt_system_external_signal.extsignals & EX_B0_PRESS) {
        opcode = temperature_get;
        app_log("PB0 Pressed.\r\n");
      }

      if (evt->data.evt_system_external_signal.extsignals & EX_B0_LONG_PRESS) {
        opcode = update_interval_set_unack;
        length = 1;
        data = periods[period_idx];
        if (period_idx == sizeof(periods) - 1) {
          period_idx = 0;
        } else {
          period_idx++;
        }
      }

      if (evt->data.evt_system_external_signal.extsignals & EX_B1_PRESS) {
        opcode = unit_get;
        app_log("PB1 Pressed.\r\n");
      }

      if (evt->data.evt_system_external_signal.extsignals & EX_B1_LONG_PRESS) {
        if (unit == celsius) {
          opcode = unit_set_unack;
          length = 1;
          data = fahrenheit;
        } else {
          opcode = unit_set;
          length = 1;
          data = celsius;
        }
        app_log("PB1 Long Pressed.\r\n");
      }

      sc = sl_btmesh_vendor_model_set_publication(my_model.elem_index,
                                                  my_model.vendor_id,
                                                  my_model.model_id,
                                                  opcode,
                                                  1,
                                                  length,
                                                  &data);
      if (sc != SL_STATUS_OK) {
        app_log("Set publication error: 0x%04lX\r\n", (unsigned long)sc);
      } else {
        app_log("Set publication done. Publishing...\r\n");
        sc = sl_btmesh_vendor_model_publish(my_model.elem_index,
                                            my_model.vendor_id,
                                            my_model.model_id);
        if (sc != SL_STATUS_OK) {
          app_log("Publish error = 0x%04lX\r\n", (unsigned long)sc);
        } else {
          app_log("Publish done.\r\n");
        }
      }
    } break;

    default:
      break;
  }
}

/**************************************************************************//**
 * Bluetooth Mesh stack event handler.
 *****************************************************************************/
void sl_btmesh_on_event(sl_btmesh_msg_t *evt)
{
  sl_status_t sc;
  char friend_status[LCD_ROW_LEN];

#if (COUNT_FRIENDSHIP_TERMINATED_EVENTS == 1)
  static int friend_term_count = 0;
#endif

  switch (SL_BT_MSG_ID(evt->header)) {

    case sl_btmesh_evt_node_initialized_id:
      app_log("\r\n--- MESH ---\r\n");

      sc = sl_btmesh_vendor_model_init(my_model.elem_index,
                                       my_model.vendor_id,
                                       my_model.model_id,
                                       my_model.publish,
                                       my_model.opcodes_len,
                                       my_model.opcodes_data);
      app_assert_status_f(sc, "Failed to initialize vendor model\r\n");

      if (evt->data.evt_node_initialized.provisioned) {
          app_log("  Provisioned : YES\r\n");
      } else {
        app_log("Node unprovisioned\r\n");

#ifdef PROV_LOCALLY
        bd_addr address;
        sc = sl_bt_system_get_identity_address(&address, 0);
        uni_addr = ((address.addr[1] << 8) | address.addr[0]) & 0x7FFF;
        app_log("Unicast Address = 0x%04X\r\n", uni_addr);
        app_log("Provisioning itself.\r\n");
        sc = sl_btmesh_node_set_provisioning_data(enc_key,
                                                  enc_key,
                                                  NET_KEY_IDX,
                                                  IVI,
                                                  uni_addr,
                                                  0);
        app_assert_status_f(sc, "Failed to provision itself\r\n");
        delay_reset_ms(100);
        break;
#else
        app_log("Send unprovisioned beacons.\r\n");
        sc = sl_btmesh_node_start_unprov_beaconing(PB_ADV | PB_GATT);
        app_assert_status_f(sc, "Failed to start unprovisioned beaconing\r\n");
#endif
      }

#ifdef PROV_LOCALLY
      uint16_t appkey_index;
      uint16_t pub_address;
      uint8_t ttl;
      uint8_t period;
      uint8_t retrans;
      uint8_t credentials;
      sc = sl_btmesh_test_get_local_model_pub(my_model.elem_index,
                                              my_model.vendor_id,
                                              my_model.model_id,
                                              &appkey_index,
                                              &pub_address,
                                              &ttl,
                                              &period,
                                              &retrans,
                                              &credentials);
      if (!sc && pub_address == CUSTOM_CTRL_GRP_ADDR) {
        app_log("Configuration done already.\r\n");
      } else {
        app_log("Pub setting result = 0x%04lX, pub setting address = 0x%04X\r\n",
                (unsigned long)sc, pub_address);

        app_log("Add local app key ...\r\n");
        sc = sl_btmesh_test_add_local_key(1, enc_key, APP_KEY_IDX, NET_KEY_IDX);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

        app_log("Bind local app key ...\r\n");
        sc = sl_btmesh_test_bind_local_model_app(my_model.elem_index,
                                                 APP_KEY_IDX,
                                                 my_model.vendor_id,
                                                 my_model.model_id);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

        app_log("Set local model pub ...\r\n");
        sc = sl_btmesh_test_set_local_model_pub(my_model.elem_index,
                                                APP_KEY_IDX,
                                                my_model.vendor_id,
                                                my_model.model_id,
                                                CUSTOM_STATUS_GRP_ADDR,
                                                DEFAULT_TTL,
                                                0, 0, 0);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

        app_log("Add local model sub ...\r\n");
        sc = sl_btmesh_test_add_local_model_sub(my_model.elem_index,
                                                my_model.vendor_id,
                                                my_model.model_id,
                                                CUSTOM_STATUS_GRP_ADDR);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

        app_log("Set relay ...\r\n");
        sc = sl_btmesh_test_set_relay(1, 0, 0);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

        app_log("Set Network tx state.\r\n");
        sc = sl_btmesh_test_set_nettx(2, 4);
        if (sc != SL_STATUS_OK) { app_log("  skipped: 0x%04lX\r\n", (unsigned long)sc); }

      }
#endif
      break;

    case sl_btmesh_evt_node_provisioned_id:
      app_log("Provisioning done.\r\n");
      break;

    case sl_btmesh_evt_node_provisioning_failed_id:
      app_log("Provisioning failed. Result = 0x%04x\r\n",
              evt->data.evt_node_provisioning_failed.result);
      break;

    case sl_btmesh_evt_node_provisioning_started_id:
      app_log("Provisioning started.\r\n");
      break;

    case sl_btmesh_evt_node_key_added_id:
      app_log("got new %s key with index %x\r\n",
              evt->data.evt_node_key_added.type == 0 ? "network " : "application ",
              evt->data.evt_node_key_added.index);
      break;

    case sl_btmesh_evt_node_config_set_id:
      app_log("evt_node_config_set_id\r\n\t");
      break;

    case sl_btmesh_evt_node_model_config_changed_id:
      app_log("model config changed, type: %d, elem_addr: %x, model_id: %x, vendor_id: %x\r\n",
              evt->data.evt_node_model_config_changed.node_config_state,
              evt->data.evt_node_model_config_changed.element_address,
              evt->data.evt_node_model_config_changed.model_id,
              evt->data.evt_node_model_config_changed.vendor_id);
      break;

    case sl_btmesh_evt_vendor_model_receive_id: {
      int32_t temperature = 0;
      sl_btmesh_evt_vendor_model_receive_t *rx_evt =
        (sl_btmesh_evt_vendor_model_receive_t *)&evt->data;

      app_log("Client: Vendor model data received.\r\n"
              "  Element index = %d\r\n"
              "  Vendor id = 0x%04X\r\n"
              "  Model id = 0x%04X\r\n"
              "  Source address = 0x%04X\r\n"
              "  Destination address = 0x%04X\r\n"
              "  Destination label UUID index = 0x%02X\r\n"
              "  App key index = 0x%04X\r\n"
              "  Non-relayed = 0x%02X\r\n"
              "  Opcode = 0x%02X\r\n"
              "  Final = 0x%04X\r\n"
              "  Payload: ",
              rx_evt->elem_index,
              rx_evt->vendor_id,
              rx_evt->model_id,
              rx_evt->source_address,
              rx_evt->destination_address,
              rx_evt->va_index,
              rx_evt->appkey_index,
              rx_evt->nonrelayed,
              rx_evt->opcode,
              rx_evt->final);

      for (int i = 0; i < evt->data.evt_vendor_model_receive.payload.len; i++) {
        app_log("%x ", evt->data.evt_vendor_model_receive.payload.data[i]);
      }
      app_log("\r\n");

      switch (rx_evt->opcode) {
        case temperature_status:
          temperature = *(uint32_t *)rx_evt->payload.data;
          app_log("Temperature = %s%d.%d %s\r\n",
                  temperature < 0 ? "-" : "",
                  abs(temperature / 1000),
                  abs(temperature % 1000),
                  unit == celsius ? (char *)"Celsius" : (char *)"Fahrenheit");
          break;

        case unit_status:
          unit = (unit_t)rx_evt->payload.data[0];
          app_log("Unit = %s\r\n",
                  unit == celsius ? (char *)"Celsius" : (char *)"Fahrenheit");
          break;

        case update_interval_status:
          update_interval = rx_evt->payload.data[0];
          app_log("Period received = 0x%d\r\n", update_interval);
          parse_period(update_interval);
          break;

        default:
          break;
      }

      app_log("\r\n");
      break;
    }

    case sl_btmesh_evt_lpn_friendship_failed_id:
      memset(friend_status, 0, LCD_ROW_LEN);
      snprintf(friend_status, LCD_ROW_LEN, "Friend Failed");
      sl_btmesh_LCD_write(friend_status, LCD_ROW_2);
      app_log("  ***Friendship Failed\r\n");
      break;

    case sl_btmesh_evt_lpn_friendship_established_id:
      memset(friend_status, 0, LCD_ROW_LEN);
      snprintf(friend_status, LCD_ROW_LEN, "Friend Est.");
      sl_btmesh_LCD_write(friend_status, LCD_ROW_2);
      sl_btmesh_LCD_write("Mesh Bonded", LCD_ROW_3);  // ADD THIS
      app_log("[LPN] Friendship established\r\n");
      break;

    case sl_btmesh_evt_lpn_friendship_terminated_id:
      memset(friend_status, 0, LCD_ROW_LEN);
      snprintf(friend_status, LCD_ROW_LEN, "Friend Term.");
      sl_btmesh_LCD_write(friend_status, LCD_ROW_2);

#if (COUNT_FRIENDSHIP_TERMINATED_EVENTS == 1)
      friend_term_count++;
      memset(friend_status, 0, LCD_ROW_LEN);
      snprintf(friend_status, LCD_ROW_LEN, "FriTermCount=%d", friend_term_count);
      sl_btmesh_LCD_write(friend_status, LCD_ROW_3);
      app_log("  ***Friendship terminated, count=%d, time=%lu ms\r\n",
              friend_term_count, (unsigned long)get_logger_timestamp());
#endif
      break;

    default:
      break;
  }
}

/**************************************************************************//**
 * Button press callback
 *****************************************************************************/
void app_button_press_cb(uint8_t button, uint8_t duration)
{
  switch (duration) {

    case APP_BUTTON_PRESS_DURATION_SHORT:
    case APP_BUTTON_PRESS_DURATION_MEDIUM:
      if (button == BUTTON_PRESS_BUTTON_0) {
        sl_bt_external_signal(EX_B0_PRESS);
      } else {
        sl_bt_external_signal(EX_B1_PRESS);
      }
      break;

    case APP_BUTTON_PRESS_DURATION_LONG:
    case APP_BUTTON_PRESS_DURATION_VERYLONG:
      if (button == BUTTON_PRESS_BUTTON_0) {
        sl_bt_external_signal(EX_B0_LONG_PRESS);
      } else {
        sl_bt_external_signal(EX_B1_LONG_PRESS);
      }
      break;

    default:
      break;
  }
}

/**************************************************************************//**
 * Reset
 *****************************************************************************/
static void factory_reset(void)
{
  char reset_status[LCD_ROW_LEN];

  app_log("factory reset\r\n");

  memset(reset_status, 0, LCD_ROW_LEN);
  snprintf(reset_status, LCD_ROW_LEN, "Factory Reset");
  sl_btmesh_LCD_write(reset_status, LCD_ROW_2);

  sl_btmesh_node_reset();

  delay_reset_ms(100);
}

static void app_reset_timer_cb(sl_simple_timer_t *handle, void *data)
{
  (void)handle;
  (void)data;
  sl_bt_system_reset(0);
}

static sl_simple_timer_t app_reset_timer;

static void delay_reset_ms(uint32_t ms)
{
  if (ms < 10) {
    ms = 10;
  }

  sl_simple_timer_start(&app_reset_timer,
                        ms,
                        app_reset_timer_cb,
                        NULL,
                        false);
}

/**************************************************************************//**
 * Update interval parse
 *****************************************************************************/
static void parse_period(uint8_t interval)
{
  switch (interval & STEP_RES_BIT_MASK) {
    case STEP_RES_100_MILLI:
      periodic_timer_ms = 100 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_1_SEC:
      periodic_timer_ms = 1000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_10_SEC:
      periodic_timer_ms = 10000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    case STEP_RES_10_MIN:
      periodic_timer_ms = 600000 * (interval & (~STEP_RES_BIT_MASK));
      break;
    default:
      break;
  }

  if (periodic_timer_ms) {
    app_log("Update period [hh:mm:ss:ms]= %02lu:%02lu:%02lu:%04lu\r\n",
            (unsigned long)(periodic_timer_ms / (1000 * 60 * 60)),
            (unsigned long)((periodic_timer_ms % (1000 * 60 * 60)) / (1000 * 60)),
            (unsigned long)((periodic_timer_ms % (1000 * 60)) / 1000),
            (unsigned long)(((periodic_timer_ms % 1000) / 1000) * 100));
  } else {
    app_log("  *** Periodic update off.\r\n");
  }
}
