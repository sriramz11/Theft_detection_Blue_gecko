/***************************************************************************//**
 * @file event_logic.h
 * @brief Alarm state machine and threshold-based event detection
 *
 * @owner Prudhvi Raj Belide
 * @project ECEN 5823 - Backpack/Locker Anti-Tamper BT Mesh System
 * @node Low-Power Node (LPN)
 *
 * Fix: Added STATE_NONE = 0xFF sentinel so app.c can initialise last_state
 *      to a value that is guaranteed to never match a real state, without
 *      casting -1 into an unsigned enum (which is undefined behavior).
 ******************************************************************************/

#ifndef EVENT_LOGIC_H
#define EVENT_LOGIC_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Alarm state enum
// ---------------------------------------------------------------------------
typedef enum {
    STATE_SAFE   = 0,
    STATE_OPEN   = 1,
    STATE_TAMPER = 2,
    STATE_NONE   = 0xFF    /* sentinel: "no state seen yet" — not a real state */
} alarm_state_t;

// ---------------------------------------------------------------------------
// Detection thresholds
// Tune these for your physical environment:
//   LIGHT_THRESHOLD : TSL2591 CH0 counts above which OPEN is triggered.
//                     100 lux in a dim room is roughly 300-600 counts at
//                     GAIN_LOW / 100 ms — start at 500 and adjust.
//   PROX_THRESHOLD  : VCNL4010 counts above which TAMPER is triggered.
//                     A hand at ~5 cm typically reads 2000-4000 counts
//                     at 200 mA LED current — start at 2000 and adjust.
// ---------------------------------------------------------------------------
#define LIGHT_THRESHOLD     500
#define PROX_THRESHOLD      4000

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/**
 * @brief  Evaluate sensor readings and return the current alarm state.
 *         Priority: TAMPER > OPEN > SAFE
 * @param  light      TSL2591 CH0 raw count
 * @param  proximity  VCNL4010 raw proximity count
 * @return alarm_state_t
 */
alarm_state_t event_logic_evaluate(uint16_t light, uint16_t proximity);

/**
 * @brief  Return a human-readable string for a given alarm state.
 * @param  state  alarm_state_t value
 * @return Null-terminated string: "SAFE", "OPEN", "TAMPER", or "UNKNOWN"
 */
const char *event_logic_state_str(alarm_state_t state);

#endif /* EVENT_LOGIC_H */
