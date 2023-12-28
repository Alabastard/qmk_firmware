// Copyright 2022 Stefan Kerkmann
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include_next <mcuconf.h>

#undef RP_PWM_USE_PWM0
#define RP_PWM_USE_PWM0 TRUE

#undef RP_PWM_USE_PWM4
#define RP_PWM_USE_PWM4 TRUE

/* dont use I2C0, as it collides with uart0, e.g. 'CONSOLE_ENABLE = yes'/debug_enable=true;
   #undef RP_I2C_USE_I2C0
   #define RP_I2C_USE_I2C0 TRUE
*/
#undef RP_I2C_USE_I2C1
#define RP_I2C_USE_I2C1 TRUE
