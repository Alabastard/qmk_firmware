/* Copyright 2022 Daniel Kao <daniel.m.kao@gmail.com>
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
#pragma once

#ifdef POINTING_VIRTKEY_MAP_ENABLE
typedef uint8_t pd_virtual_key_state_t;
enum pointing_device_virtual_keys {
    PD_VIRTKEY_UP        = 1 << 0,
    PD_VIRTKEY_LEFT      = 1 << 1,
    PD_VIRTKEY_RIGHT     = 1 << 2,
    PD_VIRTKEY_DOWN      = 1 << 3,
    PD_VIRTKEY_UNDEFINED = 1 << 7,
};

#    define POINTING_VIRTKEY_NUM_KEYS 4
/* Note 2: eight keys would be feasible: four cardinal+four diagonal, but then the uint8_t would be too small for the keystate bitmask; and the classification in 'derive_virtual_key_state' would need a minor rewrite*/

#    ifndef POINTING_VIRTKEY_DEADZONE
#        define POINTING_VIRTKEY_DEADZONE 16
#    endif
#endif
