/* Copyright 2023 Alabastard (@Alabastard-64)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef POINTING_DEVICE_MODES_ENABLE

#    include "pointing_device_modes.h"

// initialize local functions
static report_mouse_t process_pointing_mode(report_mouse_t mouse_report);
static uint8_t        get_pointing_mode_direction(report_mouse_t mouse_report);
static uint8_t        divisor_postprocess(uint8_t divisor);

// local variables
#    if POINTING_MODE_DEVICE_CONTROL_COUNT > 1
static uint8_t current_device = POINTING_MODE_DEFAULT_DEVICE;
#    else
static const uint8_t current_device  = 0;
#        if defined(POINTING_MODE_SINGLE_CONTROL)
static uint8_t       selected_device = POINTING_MODE_DEFAULT_DEVICE;
#        endif
#    endif

static uint8_t current_divisor  = POINTING_MODE_DEFAULT_DIVISOR;
static uint8_t toggle_mode_id[] = {[0 ... POINTING_MODE_DEVICE_CONTROL_COUNT - 1] = POINTING_MODE_DEFAULT};
static uint8_t pointing_mode[]  = {[0 ... POINTING_MODE_DEVICE_CONTROL_COUNT - 1] = POINTING_MODE_DEFAULT};

// set up clamping and divisor application functions
static inline int8_t clamp_int_16_to_8(int16_t value) {
    if (value < INT8_MIN) {
        return INT8_MIN;
    } else if (value > INT8_MAX) {
        return INT8_MAX;
    } else {
        return value;
    }
}

static inline int16_t clamp_int_32_to_16(int32_t value) {
    if (value < INT16_MIN) {
        return INT16_MIN;
    } else if (value > INT16_MAX) {
        return INT16_MAX;
    } else {
        return value;
    }
}

static inline uint16_t clamp_uint_16_to_8(uint16_t value) {
    if (value > UINT8_MAX) return UINT8_MAX;
    return value;
}

static int16_t divisor_multiply16(int16_t value) {
    return clamp_int_32_to_16(value * (int16_t)current_divisor);
}

static int8_t divisor_divide8(int16_t value) {
    return clamp_int_16_to_8(value / (int16_t)current_divisor);
}

static int16_t divisor_divide16(int16_t value) {
    return value / (int16_t)current_divisor;
}

/**
 * @brief divides value by current divisor return mouse x/y
 *
 * Intended for modifying pointing_mode x/y values
 *
 * @params value[in] value int16_t
 *
 * @return quotient clamped to mouse_xy_report_t range
 */
mouse_xy_report_t pointing_mode_apply_divisor_xy(int16_t value) {
#    ifdef MOUSE_EXTENDED_REPORT
    return divisor_divide16(value);
#    else
    return divisor_divide8(value);
#    endif
}

/**
 * @brief divides value by current divisor return mouse h/v
 *
 * Intended for modifying pointing_mode x/y values
 *
 * @params value[in] value int16_t
 *
 * @return quotient clamped to int8_t range
 */
int8_t pointing_mode_apply_divisor_hv(int16_t value) {
    return divisor_divide8(value);
}

/**
 * @brief Return device id of current controlled device
 *
 * @return current device id [uint8_t]
 */
uint8_t pointing_mode_get_current_device(void) {
#    ifdef POINTING_MODE_SINGLE_CONTROL
    return selected_device;
#    else
    return current_device;
#    endif
}

/**
 * @brief Change current device
 *
 * Will change which device (PM_LEFT_DEVICE, PM_RIGHT_DEVICE, etc.) is currently being modified
 *
 * NOTE: If mode is set to out of range device number device is silently set to zero (this allows cycling)
 *
 * @params[in] new side uint8_t
 */
void pointing_mode_set_current_device(uint8_t device) {
#    if (POINTING_MODE_NUM_DEVICES > 1)
    if (device > (POINTING_MODE_NUM_DEVICES - 1)) device = 0;
#        ifdef POINTING_MODE_SINGLE_CONTROL
    selected_device = device;
#        else
    current_device = device;
#        endif
#    else
    ;
#    endif
}

/**
 * @brief Reset pointing mode data
 *
 *  Clear poiting mode and set to defaults
 */
void pointing_mode_reset_mode(void) {
    pointing_mode[current_device] = toggle_mode_id[current_device];
}

/**
 * @brief access current pointing mode id
 *
 * @return uint8_t current pointing mode id
 */
uint8_t pointing_mode_get_mode(void) {
    return pointing_mode[current_device];
}

/**
 * @brief set pointing mode id
 *
 * @param[in] mode_id uint8_t
 */
void pointing_mode_set_mode(uint8_t mode_id) {
    pointing_mode[current_device] = mode_id;
}

/**
 * @brief Access current toggled pointing mode
 *
 * @return uint8_t toggle pointing mode
 */
uint8_t pointing_mode_get_toggled_mode(void) {
    return toggle_mode_id[current_device];
}

/**
 * @brief Toggle pointing mode id on/off
 *
 * Will change tg_mode_id setting to POINTING_MODE_DEFAULT when toggling "off"
 *
 * @param[in] mode_id uint8_t
 */
void pointing_mode_toggle_mode(uint8_t mode_id) {
    if (pointing_mode_get_toggled_mode() == mode_id) {
        toggle_mode_id[current_device] = POINTING_MODE_DEFAULT;
    } else {
        toggle_mode_id[current_device] = mode_id;
    }
}

/**
 * @brief Modifies divisor after
 *
 * @params direction[in] uint8_t
 *
 * @return divisor uint8_t
 */
static uint8_t divisor_postprocess(uint8_t divisor) {
    if (!(pointing_mode_divisor_postprocess_user(&divisor) && pointing_mode_divisor_postprocess_kb(&divisor))) {
        // never return without zero checking
        return divisor ? divisor : POINTING_MODE_DEFAULT_DIVISOR;
    }
    // Modify divisor if precision is toggled
    if (pointing_mode_get_toggled_mode() == PM_PRECISION && !(pointing_mode_get_mode() == PM_PRECISION)) {
        divisor = clamp_uint_16_to_8(divisor * POINTING_MODE_PRECISION_DIVISOR);
    }
    // never return without zero checking
    return divisor ? divisor : POINTING_MODE_DEFAULT_DIVISOR;
}

/**
 * @brief override current divisor value
 *
 * Will only take effect until next cycle update or next call of this process
 *
 * @param[in] divisor uint8_t
 */
void pointing_mode_divisor_override(uint8_t divisor) {
    current_divisor = divisor_postprocess(divisor);
}

/**
 * @brief local function to get pointing mode divisor
 *
 * Will handle default divisors and call weak kb and user functions
 *
 * NOTE: Expects that pointing mode and direction has been updated
 *
 * @param[in] mouse_report used to deduce the current direction
 * @return divisor uint8_t
 */
static uint8_t get_pointing_mode_divisor(report_mouse_t mouse_report) {
    uint8_t divisor = 0;
    uint8_t dir     = get_pointing_mode_direction(mouse_report);
    // allow for user and keyboard overrides
    divisor = pointing_mode_get_divisor_user(pointing_mode_get_mode(), dir);
    if (divisor) return divisor_postprocess(divisor);
    divisor = pointing_mode_get_divisor_kb(pointing_mode_get_mode(), dir);
    if (divisor) return divisor_postprocess(divisor);
    // built in divisors
    switch (pointing_mode_get_mode()) {
        case PM_PRECISION:
            divisor = POINTING_MODE_PRECISION_DIVISOR;
            break;

        case PM_DRAG:
            divisor = POINTING_MODE_DRAG_DIVISOR;
            break;

        case PM_CARET:
            divisor = dir < PD_LEFT ? POINTING_MODE_CARET_DIVISOR_V : POINTING_MODE_CARET_DIVISOR_H;
            break;

        case PM_HISTORY:
            divisor = POINTING_MODE_HISTORY_DIVISOR;
            break;

#    ifdef EXTRAKEY_ENABLE
        case PM_VOLUME:
            divisor = POINTING_MODE_VOLUME_DIVISOR;
            break;
#    endif
    }
    return divisor_postprocess(divisor);
}

/**
 * @brief local function to get single direction based on h/v
 *
 * Determines direction based on axis with largest magnitude
 *
 * Note: mouse_reports hold relative coordinates in the screen coordinate system:
 * x is left-to-right, but y is top-to-bottom; e.g. 'up' is along the negative y
 *
 * @param[in] mouse_report used to deduce the current direction
 * @return direction uint8_t
 */
static uint8_t get_pointing_mode_direction(report_mouse_t mouse_report) {
    if ((mouse_report.x == 0) && (mouse_report.y == 0)) return 0;

    if (abs(mouse_report.x) > abs(mouse_report.y)) {
        if (mouse_report.x > 0) {
            return PD_RIGHT;
        } else {
            return PD_LEFT;
        }
    } else {
        if (mouse_report.y > 0) {
            return PD_DOWN;
        } else {
            return PD_UP;
        }
    }
}

/**
 * @brief Tap keycodes based on mode axis values
 *
 * Will translate current mode x & y values into keycode taps based on divisor value.
 * Array should be ordered {DOWN, UP, LEFT, RIGHT} and will output 1 keytap/divisor
 * x and y values will be automatically updated
 *
 * NOTE: favours staying on axis and weakly favours the horizontal over the vertical axis
 *       and will clear the orthogonal axis
 *
 * @param[in] mouse_report used to deduce the current direction
 * @param[in] uint16_t* array pointer to set of four keycodes
 * @param[in] size_t array size (should be four)
 * @param[in] map_id pointing_mode_map_id, if that feature is enabled, otherwise ignored
 */
static void pointing_tap_keycodes_raw(report_mouse_t mouse_report, uint16_t* pm_keycodes, size_t size, uint8_t map_id) {
    int16_t count = 0;
    uint8_t dir   = get_pointing_mode_direction(mouse_report);
    if (dir > size) return; // error

    static uint8_t last_mode  = 0;
    static int16_t leftover_x = 0, leftover_y = 0;
    if (last_mode != pointing_mode_get_mode()) {
        leftover_x = 0;
        leftover_y = 0;
    }
    last_mode = pointing_mode_get_mode();

    switch (dir) {
        case PD_DOWN ... PD_UP:
            count = divisor_divide16(mouse_report.y + leftover_y);
            if (!count) { // exit if accumulated y is too low
                leftover_y += mouse_report.y;
                return;
            }
            leftover_y -= divisor_multiply16(count);
            leftover_x = 0;
            break;
        case PD_LEFT ... PD_RIGHT:
            count = divisor_divide16(mouse_report.x + leftover_x);
            if (!count) { // exit if accumulated x is too low
                leftover_x += mouse_report.x;
                return;
            }
            leftover_x -= divisor_multiply16(count);
            leftover_y = 0;
            break;
    }
    // skip if KC_TRNS or KC_NO (but allow for axes update above)
    // if (pm_keycodes[dir] < 2) return;

    // tap codes
    uint8_t taps = clamp_uint_16_to_8((uint16_t)abs(count));
    do {
        if (size) tap_code16_delay(pm_keycodes[dir], POINTING_MODE_TAP_DELAY);
#    ifdef POINTING_MODE_MAP_ENABLE
        else {
            action_exec(MAKE_POINTING_MODE_EVENT(map_id, dir, true));
#        if POINTING_MODE_TAP_DELAY > 0
            wait_ms(POINTING_TAP_DELAY);
#        endif // POINTING_MODE_TAP_DELAY > 0
            action_exec(MAKE_POINTING_MODE_EVENT(map_id, dir, false));
        }
#    endif
    } while (--taps);
}

/**
 * @brief external wrapper for pointing_tap_keycodes to allow easier input
 *
 * keycode order follow VIM convention (LEFT, DOWN, UP, RIGHT).s
 *
 * @param[in] mouse_report used to deduce the current direction
 * @params kc_left[in]  uint16_t keycode for negative x
 * @params kc_down[in]  uint16_t keycode for negative y
 * @params kc_up[in]    uint16_t keycode for positive y
 * @params kc_right[in] uint16_t keycode for positive x
 */
void pointing_mode_tap_codes(report_mouse_t mouse_report, uint16_t kc_left, uint16_t kc_down, uint16_t kc_up, uint16_t kc_right) {
    uint16_t pm_keycodes[4] = {kc_down, kc_up, kc_left, kc_right};
    pointing_tap_keycodes_raw(mouse_report, pm_keycodes, 4, 0);
}

#    ifdef POINTING_VIRTKEY_MAP_ENABLE
void pointing_device_modes_keys_task(pd_virtual_key_state_t keystate) {
    if (pointing_mode_get_mode() != PM_VIRTKEY) {
        return;
    }

    // invalid keystate, returned by the sensor in cases it only has invalid data available
    if (keystate == PD_VIRTKEY_UNDEFINED) return;

    static pd_virtual_key_state_t oldkeystate = 0;
    if (keystate == oldkeystate) return;

    pd_dprintf("%s keystate=0x%02x %c%c%c%c\n", __FUNCTION__, keystate, (keystate & PD_VIRTKEY_UP) ? 'U' : '_', (keystate & PD_VIRTKEY_DOWN) ? 'D' : '_', (keystate & PD_VIRTKEY_LEFT) ? 'L' : '_', (keystate & PD_VIRTKEY_RIGHT) ? 'R' : '_');

    pd_virtual_key_state_t changes = keystate ^ oldkeystate;
    oldkeystate                    = keystate;

    pd_virtual_key_state_t col_mask = 1;
    for (uint8_t col = 0; col < POINTING_VIRTKEY_NUM_KEYS; col++, col_mask <<= 1) {
        if (changes & col_mask) {
            const bool key_pressed = keystate & col_mask;
            action_exec(MAKE_POINTING_VIRTKEY_EVENT(col, key_pressed));
        }
    }
}
#    endif

/**
 * @brief Core function to handle pointing mode task
 *
 * Meant to be implemented in pointing_device_task
 *
 * @param[in] mouse_report report_mouse_t
 *
 * @return mouse_report report_mouse_t
 */
report_mouse_t pointing_device_modes_task(report_mouse_t mouse_report) {
    // skip all processing if pointing mode is PM_NONE
    if (pointing_mode_get_mode() == PM_NONE) return mouse_report;
    if (mouse_report.x == 0 && mouse_report.y == 0) return mouse_report;

#    if defined(POINTING_VIRTKEY_MAP_ENABLE)
    if (pointing_mode_get_mode() == PM_VIRTKEY) {
        // zero out the mouse report, as the input position is converted to virtual keypresses
        mouse_report.x = 0;
        mouse_report.y = 0;
        return mouse_report;
    }
#    endif

    current_divisor = get_pointing_mode_divisor(mouse_report);
    mouse_report    = process_pointing_mode(mouse_report);

    return mouse_report;
}

/**
 * @brief Handle processing of pointing modes
 *
 * Takes in and manipulates report_mouse_t, consuming parts and converting them into mode specific actions.
 *
 * @param[in] mouse_report used to deduce the current direction
 * @return mouse_report report_mouse_t
 */
static report_mouse_t process_pointing_mode(report_mouse_t mouse_report) {
    // allow overwrite of pointing modes by user, and kb
    if (!(pointing_mode_process_user(&mouse_report) && pointing_mode_process_kb(&mouse_report))) {
        return mouse_report;
    }
    switch (pointing_mode_get_mode()) {
        // precision mode  (reduce x y sensitivity temporarily)
        case PM_PRECISION:
            mouse_report.x = pointing_mode_apply_divisor_xy(mouse_report.x);
            mouse_report.y = pointing_mode_apply_divisor_xy(mouse_report.y);
            break;

        // drag scroll mode (sets mouse axes to mouse_report h & v with divisor)
        case PM_DRAG:
            mouse_report.h = pointing_mode_apply_divisor_hv(mouse_report.x);
            mouse_report.v = -pointing_mode_apply_divisor_hv(mouse_report.y);
            break;

        // caret mode (uses arrow keys to move cursor)
        case PM_CARET:
            pointing_mode_tap_codes(mouse_report, KC_LEFT, KC_DOWN, KC_UP, KC_RIGHT);
            mouse_report.x = 0;
            mouse_report.y = 0;
            break;

#    if defined(POINTING_VIRTKEY_MAP_ENABLE)
        // dpad mode (hold cursor keys in relation to current direction)
        case PM_VIRTKEY:
            mouse_report.x = 0;
            mouse_report.y = 0;
            break;
#    endif

        // history scroll mode (will scroll through undo/redo history)
        case PM_HISTORY:
            pointing_mode_tap_codes(mouse_report, C(KC_Z), KC_NO, KC_NO, C(KC_Y));
            mouse_report.x = 0;
            mouse_report.y = 0;
            break;

#    ifdef EXTRAKEY_ENABLE
        // volume scroll mode (adjusts audio volume)
        case PM_VOLUME:
            pointing_mode_tap_codes(mouse_report, KC_NO, KC_VOLD, KC_VOLU, KC_NO);
            mouse_report.x = 0;
            mouse_report.y = 0;
            break;
#    endif

#    ifdef POINTING_MODE_MAP_ENABLE
        default:
            if (pointing_mode_get_mode() > POINTING_MODE_MAP_START) {
                pointing_tap_keycodes_raw(mouse_report, NULL, 0, pointing_mode_get_mode() - POINTING_MODE_MAP_START);
                mouse_report.x = 0;
                mouse_report.y = 0;
            }
#    endif
    }
    return mouse_report;
}

/**
 * @brief User level processing of pointing device modes
 *
 * Takes a pointer to the report_mouse_t struct used in pointing mode process
 * allowing modification of mouse_report directly. The returned bool controls
 * the continued processing of pointing modes.
 *
 * NOTE: returning false will stop mode processing (for overwriting modes)
 *
 * @params mouse_report[in] pointer to report_mouse_t
 *
 * @return bool continue processing flag
 */
__attribute__((weak)) bool pointing_mode_process_user(report_mouse_t* mouse_report) {
    return true; // continue processing
}

/**
 * @brief Keyboard level processing of pointing device modes
 *
 * Takes a pointer to the report_mouse_t struct used in pointing mode process
 * allowing modification of mouse_report directly. The returned bool controls
 * the continued processing of pointing modes.
 *
 * NOTE: returning false will stop mode processing (for overwriting modes)
 *
 * @params mouse_report[in]  pointer to report_mouse_t
 *
 * @return bool continue processing flag
 */
__attribute__((weak)) bool pointing_mode_process_kb(report_mouse_t* mouse_report) {
    return true; // continue processing
}

/**
 * @brief Weak function for user level adding of pointing mode divisors
 *
 * Takes in mode id and direction allowing for divisor values to be
 * determined based on these parameters
 *
 * @params direction[in] uint8_t
 *
 * @return divisor uint8_t
 */
__attribute__((weak)) uint8_t pointing_mode_get_divisor_user(uint8_t mode_id, uint8_t direction) {
    return 0; // continue processing
}

/**
 * @brief Weak function for user level adding of pointing mode divisors
 *
 * Takes in mode id and direction allowing for divisor values to be
 * determined based on these parameters
 *
 * @params pointing_mode[in] uint8_t
 * @params direction[in] uint8_t
 *
 * @return divisor uint8_t
 */
__attribute__((weak)) uint8_t pointing_mode_get_divisor_kb(uint8_t mode_id, uint8_t direction) {
    return 0; // continue processing
}

/**
 * @brief Keyboard level modifying of divisors after being set and before use
 *
 * Allows modification the divisor after being set by get_pointing_mode_divisor stack before
 * handing off to default post processing
 *
 * @params[in] pointer to divisor uint8_t
 *
 * @return bool continue processing flag
 */
__attribute__((weak)) bool pointing_mode_divisor_postprocess_kb(uint8_t* divisor) {
    return true;
}

/**
 * @brief User level modifying of divisors after being set and before use
 *
 * Allows modification the divisor after being set by get_pointing_mode_divisor stack before
 * handing off to default post processing
 *
 * @params[in] pointer to divisor uint8_t
 *
 * @return bool continue processing flag
 */
__attribute__((weak)) bool pointing_mode_divisor_postprocess_user(uint8_t* divisor) {
    return true;
}

#endif // POINTING_DEVICE_MODES_ENABLE
