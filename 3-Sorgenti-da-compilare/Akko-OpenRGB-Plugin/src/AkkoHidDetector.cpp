/*---------------------------------------------------------*\
| AkkoHidDetector.cpp                                        |
|                                                           |
|   HID detection + virtual RGB controller registration     |
|   for the Akko 3108 V2 keyboard.                          |
|                                                           |
|   The OpenRGB plugin API v5 does not expose a             |
|   RegisterDeviceDetector hook, so the plugin runs its own |
|   hid_enumerate scan (at Load() and on RescanDevices)     |
|   and registers the device through                       |
|   CreateVirtualRGBController + RegisterVirtualRGBController,|
|   which lands in the same ResourceManager device list as  |
|   a core-detected device.                                 |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#include "AkkoHidDetector.h"
#include "AkkoDeviceBridge.h"

#include <hidapi.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <algorithm>

/*---------------------------------------------------------*\
| Logging helper: writes to OpenRGB log and terminal        |
\*---------------------------------------------------------*/
void AkkoLog(OpenRGBPluginAPIInterface* api, unsigned int level,
             const char* fmt, ...)
{
    char msg[1024];
    va_list args;

    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if(api != nullptr)
    {
        api->LogEntry(__FILE__, __LINE__, level, "%s", msg);
    }

    /* Also print to the terminal so detection is visible even
       when running OpenRGB from a console. */
    switch(level)
    {
        case AKKO_LOG_ERROR:
            std::fprintf(stderr, "[AkkoPlugin] ERROR: %s\n", msg);
            break;
        case AKKO_LOG_WARNING:
            std::fprintf(stderr, "[AkkoPlugin] WARNING: %s\n", msg);
            break;
        default:
            std::printf("[AkkoPlugin] %s\n", msg);
            break;
    }
    std::fflush(stdout);
    std::fflush(stderr);
}

/*---------------------------------------------------------*\
| LED key names (KEY_EN_* literals, same strings used by    |
| the OpenRGB RGBControllerKeyNames table so that the       |
| DeviceView label lookup recognizes them).                 |
\*---------------------------------------------------------*/
static const char* led_key_names[AKKO_BRIDGE_NUM_LEDS] =
{
    /* 0-12: Esc + F1-F12 */
    "KEY_EN_ESCAPE",
    "KEY_EN_F1",  "KEY_EN_F2",  "KEY_EN_F3",  "KEY_EN_F4",
    "KEY_EN_F5",  "KEY_EN_F6",  "KEY_EN_F7",  "KEY_EN_F8",
    "KEY_EN_F9",  "KEY_EN_F10", "KEY_EN_F11", "KEY_EN_F12",
    /* 13-19 */
    "KEY_EN_PRINT_SCREEN", "KEY_EN_SCROLL_LOCK", "KEY_EN_PAUSE_BREAK",
    "KEY_EN_UNUSED",              /* Calculator key                     */
    "KEY_EN_MEDIA_VOLUME_DOWN", "KEY_EN_MEDIA_VOLUME_UP", "KEY_EN_MEDIA_MUTE",
    /* 20-33 */
    "KEY_EN_BACK_TICK",
    "KEY_EN_1", "KEY_EN_2", "KEY_EN_3", "KEY_EN_4", "KEY_EN_5",
    "KEY_EN_6", "KEY_EN_7", "KEY_EN_8", "KEY_EN_9", "KEY_EN_0",
    "KEY_EN_MINUS", "KEY_EN_EQUALS", "KEY_EN_BACKSPACE",
    /* 34-36 */
    "KEY_EN_INSERT", "KEY_EN_HOME", "KEY_EN_PAGE_UP",
    /* 37-40 */
    "KEY_EN_NUMPAD_LOCK", "KEY_EN_NUMPAD_DIVIDE",
    "KEY_EN_NUMPAD_TIMES", "KEY_EN_NUMPAD_MINUS",
    /* 41-54 */
    "KEY_EN_TAB",
    "KEY_EN_Q", "KEY_EN_W", "KEY_EN_E", "KEY_EN_R", "KEY_EN_T",
    "KEY_EN_Y", "KEY_EN_U", "KEY_EN_I", "KEY_EN_O", "KEY_EN_P",
    "KEY_EN_LEFT_BRACKET", "KEY_EN_RIGHT_BRACKET", "KEY_EN_ANSI_BACK_SLASH",
    /* 55-57 */
    "KEY_EN_DELETE", "KEY_EN_END", "KEY_EN_PAGE_DOWN",
    /* 58-61 */
    "KEY_EN_NUMPAD_7", "KEY_EN_NUMPAD_8", "KEY_EN_NUMPAD_9", "KEY_EN_NUMPAD_PLUS",
    /* 62-74 */
    "KEY_EN_CAPS_LOCK",
    "KEY_EN_A", "KEY_EN_S", "KEY_EN_D", "KEY_EN_F", "KEY_EN_G",
    "KEY_EN_H", "KEY_EN_J", "KEY_EN_K", "KEY_EN_L",
    "KEY_EN_SEMICOLON", "KEY_EN_QUOTE", "KEY_EN_ANSI_ENTER",
    /* 75-77 */
    "KEY_EN_NUMPAD_4", "KEY_EN_NUMPAD_5", "KEY_EN_NUMPAD_6",
    /* 78-89 */
    "KEY_EN_LEFT_SHIFT",
    "KEY_EN_Z", "KEY_EN_X", "KEY_EN_C", "KEY_EN_V", "KEY_EN_B",
    "KEY_EN_N", "KEY_EN_M",
    "KEY_EN_COMMA", "KEY_EN_PERIOD", "KEY_EN_FORWARD_SLASH",
    "KEY_EN_RIGHT_SHIFT",
    /* 90-94 */
    "KEY_EN_NUMPAD_1", "KEY_EN_NUMPAD_2", "KEY_EN_NUMPAD_3",
    "KEY_EN_NUMPAD_0", "KEY_EN_NUMPAD_PERIOD",
    /* 95-98 */
    "KEY_EN_LEFT_CONTROL", "KEY_EN_LEFT_WINDOWS", "KEY_EN_LEFT_ALT",
    "KEY_EN_SPACE",
    /* 99-102 */
    "KEY_EN_RIGHT_ALT", "KEY_EN_RIGHT_FUNCTION", "KEY_EN_RIGHT_CONTROL",
    "KEY_EN_MENU",
    /* 103-106 */
    "KEY_EN_LEFT_ARROW", "KEY_EN_UP_ARROW",
    "KEY_EN_RIGHT_ARROW", "KEY_EN_DOWN_ARROW",
    /* 107 */
    "KEY_EN_NUMPAD_ENTER"
};

/*---------------------------------------------------------*\
| Matrix map: 6 rows x 22 columns (OpenRGB LED indices)     |
\*---------------------------------------------------------*/
#ifndef NA
#define NA 0xFFFFFFFF
#endif
static const unsigned int matrix_map[6][22] =
{
    {   0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,   NA,   NA },
    {  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,    NA },
    {  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,    NA },
    {  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,   NA,   NA,   NA,   NA,   NA,   NA },
    {  78,  79,  80,  81,  82,  83,  84,  85,  86,  87,  88,  89, 104,  90,  91,  92, 107,   NA,   NA,   NA,   NA,   NA },
    {  95,  96,  97,  98,  99, 100, 102, 101, 103, 105, 106,  93,  94,   NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA }
};

/*---------------------------------------------------------*\
| Build the RGBController_Setup describing the Akko 3108 V2 |
| (same zones/modes as the core driver).                    |
\*---------------------------------------------------------*/
static void AkkoBuildSetup(RGBController_Setup& setup,
                           AkkoDeviceBridge* bridge,
                           const AkkoDeviceInfo& info)
{
    setup.name          = "Akko 3108 V2";
    setup.vendor        = "Akko";
    setup.description   = "Akko 3108 V2 Keyboard Device (Keyboard H / H3108 V2)";
    setup.location      = info.path;
    setup.serial        = info.serial;
    setup.version       = "";
    setup.configuration = "";
    setup.type          = DEVICE_TYPE_KEYBOARD;
    setup.flags         = 0;
    setup.active_mode   = 0;            /* Custom (per-key) */

    /*-----------------------------------------------------*\
    | Modes (device level)                                  |
    \*-----------------------------------------------------*/
    mode m;

    /* Custom */
    m.name          = "Custom";
    m.value         = AKKO_BRIDGE_MODE_CUSTOM;
    m.flags         = MODE_FLAG_HAS_PER_LED_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.color_mode    = MODE_COLORS_PER_LED;
    setup.modes.push_back(m);

    /* Off */
    m               = mode();
    m.name          = "Off";
    m.value         = AKKO_BRIDGE_MODE_OFF;
    m.flags         = MODE_FLAG_AUTOMATIC_SAVE;
    m.color_mode    = MODE_COLORS_NONE;
    setup.modes.push_back(m);

    /* Color Wave */
    m               = mode();
    m.name          = "Color Wave";
    m.value         = AKKO_BRIDGE_MODE_COLOR_WAVE_LONG;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.direction     = MODE_DIRECTION_LEFT;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Color Wave (Short) */
    m               = mode();
    m.name          = "Color Wave (Short)";
    m.value         = AKKO_BRIDGE_MODE_COLOR_WAVE_SHORT;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.direction     = MODE_DIRECTION_LEFT;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Color Wheel */
    m               = mode();
    m.name          = "Color Wheel";
    m.value         = AKKO_BRIDGE_MODE_COLOR_WHEEL;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_LR | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.direction     = MODE_DIRECTION_LEFT;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Spectrum Cycle */
    m               = mode();
    m.name          = "Spectrum Cycle";
    m.value         = AKKO_BRIDGE_MODE_SPECTRUM_CYCLE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.color_mode    = MODE_COLORS_NONE;
    setup.modes.push_back(m);

    /* Breathing */
    m               = mode();
    m.name          = "Breathing";
    m.value         = AKKO_BRIDGE_MODE_BREATHING;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Hurricane */
    m               = mode();
    m.name          = "Hurricane";
    m.value         = AKKO_BRIDGE_MODE_HURRICANE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Accumulate */
    m               = mode();
    m.name          = "Accumulate";
    m.value         = AKKO_BRIDGE_MODE_ACCUMULATE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Starlight (Fast) */
    m               = mode();
    m.name          = "Starlight";
    m.value         = AKKO_BRIDGE_MODE_STARLIGHT_FAST;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Starlight (Slow) */
    m               = mode();
    m.name          = "Starlight (Slow)";
    m.value         = AKKO_BRIDGE_MODE_STARLIGHT_SLOW;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Visor */
    m               = mode();
    m.name          = "Visor";
    m.value         = AKKO_BRIDGE_MODE_VISOR;
    m.flags         = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Static */
    m               = mode();
    m.name          = "Static";
    m.value         = AKKO_BRIDGE_MODE_STATIC;
    m.flags         = MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_AUTOMATIC_SAVE;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Rainbow Circle */
    m               = mode();
    m.name          = "Rainbow Circle";
    m.value         = AKKO_BRIDGE_MODE_RAINBOW_WAVE_CIRC;
    m.flags         = MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_AUTOMATIC_SAVE;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.color_mode    = MODE_COLORS_RANDOM;
    setup.modes.push_back(m);

    /* Vertical Rainbow */
    m               = mode();
    m.name          = "Vertical Rainbow";
    m.value         = AKKO_BRIDGE_MODE_RAINBOW_WAVE_VERT;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_DIRECTION_UD | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.direction     = MODE_DIRECTION_UP;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Blooming */
    m               = mode();
    m.name          = "Blooming";
    m.value         = AKKO_BRIDGE_MODE_BLOOMING;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.color_mode    = MODE_COLORS_RANDOM;
    setup.modes.push_back(m);

    /* Reactive */
    m               = mode();
    m.name          = "Reactive";
    m.value         = AKKO_BRIDGE_MODE_REACTIVE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Reactive Ripple */
    m               = mode();
    m.name          = "Reactive Ripple";
    m.value         = AKKO_BRIDGE_MODE_REACTIVE_RIPPLE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /* Reactive Line */
    m               = mode();
    m.name          = "Reactive Line";
    m.value         = AKKO_BRIDGE_MODE_REACTIVE_LINE;
    m.flags         = MODE_FLAG_HAS_SPEED | MODE_FLAG_HAS_MODE_SPECIFIC_COLOR | MODE_FLAG_HAS_RANDOM_COLOR | MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_AUTOMATIC_SAVE;
    m.speed_min     = AKKO_BRIDGE_SPEED_SLOWEST;
    m.speed_max     = AKKO_BRIDGE_SPEED_FASTEST;
    m.speed         = AKKO_BRIDGE_SPEED_NORMAL;
    m.brightness_min= AKKO_BRIDGE_BRIGHTNESS_LOWEST;
    m.brightness_max= AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.brightness    = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    m.colors_min    = 1;
    m.colors_max    = 1;
    m.color_mode    = MODE_COLORS_MODE_SPECIFIC;
    m.colors.resize(1);
    m.colors[0]     = 0x000000FF;
    setup.modes.push_back(m);

    /*-----------------------------------------------------*\
    | Zones: single MATRIX zone with 108 LEDs               |
    \*-----------------------------------------------------*/
    zone z;

    z.name          = "Keyboard";
    z.type          = ZONE_TYPE_MATRIX;
    z.leds_min      = AKKO_BRIDGE_NUM_LEDS;
    z.leds_max      = AKKO_BRIDGE_NUM_LEDS;
    z.leds_count    = AKKO_BRIDGE_NUM_LEDS;
    z.matrix_map.Set(6, 22, (unsigned int*)&matrix_map);

    setup.zones.push_back(z);

    /*-----------------------------------------------------*\
    | LEDs                                                  |
    \*-----------------------------------------------------*/
    for(int led_idx = 0; led_idx < AKKO_BRIDGE_NUM_LEDS; led_idx++)
    {
        led new_led;

        new_led.name  = led_key_names[led_idx];
        new_led.value = led_idx;

        setup.leds.push_back(new_led);
    }

    /*-----------------------------------------------------*\
    | Bridge object + callback function pointers            |
    \*-----------------------------------------------------*/
    setup.object_ptr     = bridge;
    setup.DeviceConfigureZone                 = AkkoDeviceBridge::DeviceConfigureZone;
    setup.DeviceUpdateLEDs                    = AkkoDeviceBridge::DeviceUpdateLEDs;
    setup.DeviceUpdateZoneLEDs                = AkkoDeviceBridge::DeviceUpdateZoneLEDs;
    setup.DeviceUpdateSingleLED               = AkkoDeviceBridge::DeviceUpdateSingleLED;
    setup.DeviceUpdateMode                    = AkkoDeviceBridge::DeviceUpdateMode;
    setup.DeviceSaveMode                      = AkkoDeviceBridge::DeviceSaveMode;
    setup.DeviceUpdateZoneMode                = AkkoDeviceBridge::DeviceUpdateZoneMode;
    setup.DeviceUpdateDeviceSpecificConfiguration      = nullptr;
    setup.DeviceUpdateDeviceSpecificZoneConfiguration  = nullptr;
}

/*---------------------------------------------------------*\
| Log every HID device on the system (debug)                |
\*---------------------------------------------------------*/
bool AkkoLogAllHidDevices(OpenRGBPluginAPIInterface* api)
{
    bool akko_present = false;

    struct hid_device_info* devs = hid_enumerate(0x0000, 0x0000);
    struct hid_device_info* cur  = devs;

    int count = 0;

    while(cur != nullptr)
    {
        count++;

        AkkoLog(api, AKKO_LOG_DEBUG,
                "HID device #%d: VID=0x%04X PID=0x%04X interface=%d "
                "usage_page=0x%04X usage=0x%04X path=%s vendor='%ls' product='%ls'",
                count,
                cur->vendor_id, cur->product_id,
                cur->interface_number,
                cur->usage_page, cur->usage,
                cur->path,
                (cur->manufacturer_string != nullptr) ? cur->manufacturer_string : L"",
                (cur->product_string    != nullptr) ? cur->product_string    : L"");

        if((cur->vendor_id == AKKO_3108_V2_VID) && (cur->product_id == AKKO_3108_V2_PID))
        {
            akko_present = true;
        }

        cur = cur->next;
    }

    hid_free_enumeration(devs);

    AkkoLog(api, AKKO_LOG_DEBUG,
            "HID enumeration complete: %d device(s), Akko 3108 V2 present: %s",
            count, akko_present ? "YES" : "NO");

    return(akko_present);
}

/*---------------------------------------------------------*\
| Filter a full enumeration into Akko interfaces            |
\*---------------------------------------------------------*/
std::vector<AkkoDeviceInfo> AkkoEnumerateKeyboardFromAll(hid_device_info* all_devs)
{
    std::vector<AkkoDeviceInfo> result;

    for(hid_device_info* cur = all_devs; cur != nullptr; cur = cur->next)
    {
        if((cur->vendor_id != AKKO_3108_V2_VID) || (cur->product_id != AKKO_3108_V2_PID))
        {
            continue;
        }

        AkkoDeviceInfo info;

        info.path            = (cur->path != nullptr) ? cur->path : "";
        info.interface_number= cur->interface_number;
        info.usage_page      = cur->usage_page;
        info.usage           = cur->usage;
        info.serial          = (cur->serial_number != nullptr)
                             ? "" /* avoid UTF conversion issues here */
                             : "";
        info.product         = (cur->product_string != nullptr) ? "" : "";
        info.manufacturer    = (cur->manufacturer_string != nullptr) ? "" : "";

        result.push_back(info);
    }

    return(result);
}

/*---------------------------------------------------------*\
| Enumerate the Akko keyboard interfaces                    |
\*---------------------------------------------------------*/
std::vector<AkkoDeviceInfo> AkkoEnumerateKeyboard(OpenRGBPluginAPIInterface* api)
{
    std::vector<AkkoDeviceInfo> result;

    struct hid_device_info* devs = hid_enumerate(AKKO_3108_V2_VID, AKKO_3108_V2_PID);

    AkkoLog(api, AKKO_LOG_DEBUG,
            "Akko 3108 V2: hid_enumerate(0x0C45, 0x762B) returned %s",
            (devs != nullptr) ? "interfaces" : "nothing");

    for(hid_device_info* cur = devs; cur != nullptr; cur = cur->next)
    {
        AkkoDeviceInfo info;

        info.path            = (cur->path != nullptr) ? cur->path : "";
        info.interface_number= cur->interface_number;
        info.usage_page      = cur->usage_page;
        info.usage           = cur->usage;
        info.serial          = (cur->serial_number != nullptr)
                             ? "" /* reserved for future UTF support */
                             : "";
        info.product         = (cur->product_string != nullptr) ? "" : "";
        info.manufacturer    = (cur->manufacturer_string != nullptr) ? "" : "";

        AkkoLog(api, AKKO_LOG_DEBUG,
                "Akko interface found: interface=%d usage_page=0x%04X usage=0x%04X path=%s",
                info.interface_number, info.usage_page, info.usage, info.path.c_str());

        result.push_back(info);
    }

    /* Sort by preference FIRST (best interface at the front),
       then drop duplicate (path, interface) entries keeping the
       best one: the LED-control interface is usage_page 0xFF1C
       on interface 1 (same as the official EVision detector). */
    std::stable_sort(result.begin(), result.end(),
                     [](const AkkoDeviceInfo& a, const AkkoDeviceInfo& b)
                     {
                         auto score = [](const AkkoDeviceInfo& i)
                         {
                             if((i.interface_number == AKKO_KEYBOARD_INTERFACE)
                             && (i.usage_page == AKKO_KEYBOARD_USAGE_PAGE))
                                 return 0;
                             if(i.usage_page == AKKO_KEYBOARD_USAGE_PAGE)
                                 return 1;
                             if(i.usage_page == 0xFF00)          /* vendor page */
                                 return 2;
                             if(i.interface_number > 0)          /* secondary iface */
                                 return 3;
                             return 4;                           /* last resort     */
                         };
                         return(score(a) < score(b));
                     });

    result.erase(std::unique(result.begin(), result.end(),
                             [](const AkkoDeviceInfo& a, const AkkoDeviceInfo& b)
                             { return(a.path == b.path
                                   && a.interface_number == b.interface_number); }),
                 result.end());

    hid_free_enumeration(devs);

    return(result);
}

/*---------------------------------------------------------*\
| Full detection + registration                             |
\*---------------------------------------------------------*/
RGBControllerInterface* AkkoDetectAndRegister(OpenRGBPluginAPIInterface* api,
                                              AkkoDeviceBridge** bridge_out)
{
    if(api == nullptr)
    {
        return(nullptr);
    }

    AkkoLog(api, AKKO_LOG_INFO, "Starting Akko 3108 V2 HID detection...");

    /* 1. Debug listing of every HID device on the system */
    AkkoLogAllHidDevices(api);

    /* 2. Enumerate the Akko keyboard's USB interfaces */
    std::vector<AkkoDeviceInfo> interfaces = AkkoEnumerateKeyboard(api);

    /* 3. Already detected by the OpenRGB core (custom driver
          build)? Then do not register a second device. */
    std::vector<RGBControllerInterface*> existing = api->GetRGBControllers();

    for(RGBControllerInterface* ctrl : existing)
    {
        std::string name = ctrl->GetName();

        if((name.find("Akko") != std::string::npos)
        && (name.find("3108") != std::string::npos))
        {
            AkkoLog(api, AKKO_LOG_INFO,
                    "Akko 3108 V2 already detected by the OpenRGB core "
                    "('%s'): the plugin editor will use that device.",
                    name.c_str());
            return(nullptr);
        }
    }

    if(interfaces.empty())
    {
        AkkoLog(api, AKKO_LOG_WARNING,
                "Akko 3108 V2 not found on USB (VID=0x0C45 PID=0x762B). "
                "Is the keyboard plugged in?");
        return(nullptr);
    }

    /* 4. Try to open the best candidate interface */
    hid_device* dev = nullptr;
    AkkoDeviceInfo chosen;

    for(const AkkoDeviceInfo& candidate : interfaces)
    {
        AkkoLog(api, AKKO_LOG_INFO,
                "Trying hid_open_path(interface %d, usage_page 0x%04X): %s",
                candidate.interface_number, candidate.usage_page,
                candidate.path.c_str());

        dev = hid_open_path(candidate.path.c_str());

        if(dev != nullptr)
        {
            chosen = candidate;
            AkkoLog(api, AKKO_LOG_INFO,
                    "hid_open SUCCESS on interface %d (usage_page 0x%04X, usage 0x%04X)",
                    candidate.interface_number, candidate.usage_page, candidate.usage);
            break;
        }
        else
        {
            AkkoLog(api, AKKO_LOG_WARNING,
                    "hid_open FAILED on interface %d (usage_page 0x%04X): %ls",
                    candidate.interface_number, candidate.usage_page,
                    hid_error(nullptr));
        }
    }

    if(dev == nullptr)
    {
        AkkoLog(api, AKKO_LOG_ERROR,
                "Unable to open any Akko 3108 V2 HID interface. "
                "Check permissions (/dev/hidraw*, udev rules) or try as root.");
        return(nullptr);
    }

    /* 5. Build bridge + virtual controller */
    AkkoDeviceBridge* bridge = new AkkoDeviceBridge(dev, chosen.path);

    RGBController_Setup setup;   /* default-constructed: strings/vectors validi */
                                 /* NON usare memset: romperebbe SSO std::string */
    AkkoBuildSetup(setup, bridge, chosen);

    /* Cache the mode values (same order as setup.modes) so the
       bridge can map mode index -> protocol value. */
    {
        std::vector<int> values;

        for(const mode& m : setup.modes)
        {
            values.push_back(m.value);
        }

        bridge->SetModeValues(values);
    }

    RGBControllerInterface* rgb = api->CreateVirtualRGBController(&setup);

    if(rgb == nullptr)
    {
        AkkoLog(api, AKKO_LOG_ERROR, "CreateVirtualRGBController failed.");
        delete bridge;
        return(nullptr);
    }

    bridge->SetController(rgb);

    /* 6. Register (from this background thread, direct call is
          safe; use RegisterVirtualRGBControllerInThread for extra
          safety when called from a UI thread). */
    api->RegisterVirtualRGBController(rgb);

    /* The caller becomes the owner of the bridge (the core only
       frees the controller itself). It must be deleted after
       DeleteVirtualRGBController. */
    if(bridge_out != nullptr)
    {
        *bridge_out = bridge;
    }

    AkkoLog(api, AKKO_LOG_INFO,
            "Registered 'Akko 3108 V2' (interface %d, %zu modes, %zu LEDs). "
            "Per-key colors and effects now go over HID.",
            chosen.interface_number, setup.modes.size(), setup.leds.size());

    return(rgb);
}