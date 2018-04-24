/*
WS2812FX.h - Library for WS2812 LED effects.

Harm Aldick - 2016
www.aldick.org

Ported to esp-open-rtos by PCSaito - 2018
www.github.com/pcsaito


FEATURES
* A lot of blinken modes and counting


LICENSE
The MIT License (MIT)
Copyright (c) 2016  Harm Aldick
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
CHANGELOG
2016-05-28   Initial beta release
2016-06-03   Code cleanup, minor improvements, new modes
2016-06-04   2 new fx, fixed setColor (now also resets _mode_color)
2018-04-24   ported to esp-open-rtos to use in esp-homekit-demo
*/

#ifndef WS2812FX_h
#define WS2812FX_h

#include "FreeRTOS.h"
#include "ws2812_i2s/ws2812_i2s.h"

#define LED_INBUILT_GPIO 2      // this is the onboard LED used to show on/off only

#define DEFAULT_MODE 9
#define DEFAULT_SPEED 1
#define DEFAULT_COLOR 0xFF10EE

#define SPEED_MIN 1
#define SPEED_MAX 255

#define BRIGHTNESS_MIN 0
#define BRIGHTNESS_MAX 255
#define BRIGHTNESS_FILTER 0.9

#define MODE_COUNT 54

#define FX_MODE_STATIC                   0
#define FX_MODE_BLINK                    1
#define FX_MODE_BREATH                   2
#define FX_MODE_COLOR_WIPE               3
#define FX_MODE_COLOR_WIPE_RANDOM        4
#define FX_MODE_RANDOM_COLOR             5
#define FX_MODE_SINGLE_DYNAMIC           6
#define FX_MODE_MULTI_DYNAMIC            7
#define FX_MODE_RAINBOW                  8
#define FX_MODE_RAINBOW_CYCLE            9
#define FX_MODE_SCAN                    10
#define FX_MODE_DUAL_SCAN               11
#define FX_MODE_FADE                    12
#define FX_MODE_THEATER_CHASE           13
#define FX_MODE_THEATER_CHASE_RAINBOW   14
#define FX_MODE_RUNNING_LIGHTS          15
#define FX_MODE_TWINKLE                 16
#define FX_MODE_TWINKLE_RANDOM          17
#define FX_MODE_TWINKLE_FADE            18
#define FX_MODE_TWINKLE_FADE_RANDOM     19
#define FX_MODE_SPARKLE                 20
#define FX_MODE_FLASH_SPARKLE           21
#define FX_MODE_HYPER_SPARKLE           22
#define FX_MODE_STROBE                  23
#define FX_MODE_STROBE_RAINBOW          24
#define FX_MODE_MULTI_STROBE            25
#define FX_MODE_BLINK_RAINBOW           26
#define FX_MODE_CHASE_WHITE             27
#define FX_MODE_CHASE_COLOR             28
#define FX_MODE_CHASE_RANDOM            29
#define FX_MODE_CHASE_RAINBOW           30
#define FX_MODE_CHASE_FLASH             31
#define FX_MODE_CHASE_FLASH_RANDOM      32
#define FX_MODE_CHASE_RAINBOW_WHITE     33
#define FX_MODE_CHASE_BLACKOUT          34
#define FX_MODE_CHASE_BLACKOUT_RAINBOW  35
#define FX_MODE_COLOR_SWEEP_RANDOM      36
#define FX_MODE_RUNNING_COLOR           37
#define FX_MODE_RUNNING_RED_BLUE        38
#define FX_MODE_RUNNING_RANDOM          39
#define FX_MODE_LARSON_SCANNER          40
#define FX_MODE_COMET                   41
#define FX_MODE_FIREWORKS               42
#define FX_MODE_FIREWORKS_RANDOM        43
#define FX_MODE_MERRY_CHRISTMAS         44
#define FX_MODE_FIRE_FLICKER            45
#define FX_MODE_FIRE_FLICKER_SOFT       46
#define FX_MODE_FIRE_FLICKER_INTENSE    47
#define FX_MODE_DUAL_COLOR_WIPE_IN_OUT  48
#define FX_MODE_DUAL_COLOR_WIPE_IN_IN   49
#define FX_MODE_DUAL_COLOR_WIPE_OUT_OUT 50
#define FX_MODE_DUAL_COLOR_WIPE_OUT_IN  51
#define FX_MODE_CIRCUS_COMBUSTUS        52
#define FX_MODE_HALLOWEEN               53

typedef void (*mode)(void);
  
void
	WS2812FX_init(uint16_t pixel_count),
	WS2812FX_initModes(void),
	WS2812FX_service(void *_args),
	WS2812FX_start(void),
	WS2812FX_stop(void),
	WS2812FX_setMode(uint8_t m),
	WS2812FX_setMode360(float m),
	WS2812FX_setSpeed(uint8_t s),
	WS2812FX_setColor(uint8_t r, uint8_t g, uint8_t b),
	WS2812FX_setColor32(uint32_t c),
	WS2812FX_setBrightness(uint8_t b);

bool
	WS2812FX_isRunning(void);

uint8_t
	WS2812FX_getMode(void),
	WS2812FX_getSpeed(void),
	WS2812FX_getBrightness(void),
	WS2812FX_getModeCount(void);

uint16_t
	WS2812FX_getLength(void);

uint32_t
	WS2812FX_color_wheel(uint8_t),
	WS2812FX_getColor(void);

//private
void
	WS2812FX_strip_off(void),
	WS2812FX_mode_static(void),
	WS2812FX_mode_blink(void),
	WS2812FX_mode_color_wipe(void),
	WS2812FX_mode_color_wipe_random(void),
	WS2812FX_mode_random_color(void),
	WS2812FX_mode_single_dynamic(void),
	WS2812FX_mode_multi_dynamic(void),
	WS2812FX_mode_breath(void),
	WS2812FX_mode_fade(void),
	WS2812FX_mode_scan(void),
	WS2812FX_mode_dual_scan(void),
	WS2812FX_mode_theater_chase(void),
	WS2812FX_mode_theater_chase_rainbow(void),
	WS2812FX_mode_rainbow(void),
	WS2812FX_mode_rainbow_cycle(void),
	WS2812FX_mode_running_lights(void),
	WS2812FX_mode_twinkle(void),
	WS2812FX_mode_twinkle_random(void),
	WS2812FX_mode_twinkle_fade(void),
	WS2812FX_mode_twinkle_fade_random(void),
	WS2812FX_mode_sparkle(void),
	WS2812FX_mode_flash_sparkle(void),
	WS2812FX_mode_hyper_sparkle(void),
	WS2812FX_mode_strobe(void),
	WS2812FX_mode_strobe_rainbow(void),
	WS2812FX_mode_multi_strobe(void),
	WS2812FX_mode_blink_rainbow(void),
	WS2812FX_mode_chase_white(void),
	WS2812FX_mode_chase_color(void),
	WS2812FX_mode_chase_random(void),
	WS2812FX_mode_chase_rainbow(void),
	WS2812FX_mode_chase_flash(void),
	WS2812FX_mode_chase_flash_random(void),
	WS2812FX_mode_chase_rainbow_white(void),
	WS2812FX_mode_chase_blackout(void),
	WS2812FX_mode_chase_blackout_rainbow(void),
	WS2812FX_mode_color_sweep_random(void),
	WS2812FX_mode_running_color(void),
	WS2812FX_mode_running_red_blue(void),
	WS2812FX_mode_running_random(void),
	WS2812FX_mode_larson_scanner(void),
	WS2812FX_mode_comet(void),
	WS2812FX_mode_fireworks(void),
	WS2812FX_mode_fireworks_random(void),
	WS2812FX_mode_merry_christmas(void),
	WS2812FX_mode_fire_flicker(void),
	WS2812FX_mode_fire_flicker_soft(void),
	WS2812FX_mode_fire_flicker_intense(void),
	WS2812FX_mode_fire_flicker_int(int),
	WS2812FX_mode_dual_color_wipe_in_out(void),
	WS2812FX_mode_dual_color_wipe_in_in(void),
	WS2812FX_mode_dual_color_wipe_out_out(void),
	WS2812FX_mode_dual_color_wipe_out_in(void),
	WS2812FX_mode_circus_combustus(void),
	WS2812FX_mode_halloween(void);

#endif
