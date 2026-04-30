/***************************************************************************//**
 * @file event_logic.c
 * @brief Alarm state machine and threshold-based event detection
 *
 * @owner Prudhvi Raj Belide
 * @project ECEN 5823 - Backpack/Locker Anti-Tamper BT Mesh System
 * @node Low-Power Node (LPN)
 *
 * Detection priority: TAMPER > OPEN > SAFE
 *
 *   If proximity is above threshold AND light is above threshold, TAMPER wins.
 *   This matches the proposal: proximity is the stronger tamper signal.
 *   OPEN is only reported when there is light exposure but no close-range
 *   hand/object presence.
 ******************************************************************************/

#include "event_logic.h"

/**************************************************************************//**
 * event_logic_evaluate
 *****************************************************************************/
alarm_state_t event_logic_evaluate(uint16_t light, uint16_t proximity)
{
    /*
     * Priority order: TAMPER > OPEN > SAFE
     *
     * Check proximity first: a hand near the opening is a stronger signal
     * than a light change and should override OPEN if both trigger at once.
     */
    if (proximity > PROX_THRESHOLD) {
        return STATE_TAMPER;
    }
    if (light > LIGHT_THRESHOLD) {
        return STATE_OPEN;
    }
    return STATE_SAFE;
}

/**************************************************************************//**
 * event_logic_state_str
 *****************************************************************************/
const char *event_logic_state_str(alarm_state_t state)
{
    switch (state) {
        case STATE_SAFE:   return "SAFE";
        case STATE_OPEN:   return "OPEN";
        case STATE_TAMPER: return "TAMPER";
        case STATE_NONE:   return "NONE";
        default:           return "UNKNOWN";
    }
}
