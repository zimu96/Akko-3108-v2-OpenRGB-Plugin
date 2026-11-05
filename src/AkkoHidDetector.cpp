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
| Nomi dei LED mostrati nella pagina Dispositivi, in        |
| italiano e leggibili (es. "Ctrl destro", "Freccia su").    |
\*---------------------------------------------------------*/
static const char* led_key_names[AKKO_BRIDGE_NUM_LEDS] =
{
    "Esc", "F1", "F2", "F3",
    "F4", "F5", "F6", "F7",
    "F8", "F9", "F10", "F11",
    "F12", "Stamp (PrtSc)", "Bloc Scorr (ScrLk)", "Pausa (Pause)",
    "Calcolatrice", "Volume -", "Volume +", "Muto (Mute)",
    "` (apice inverso)", "1", "2", "3",
    "4", "5", "6", "7",
    "8", "9", "0", "- (meno)",
    "= (uguale)", "Backspace", "Ins", "Home",
    "Pag su (PgUp)", "Tastierino Bloc Num", "Tastierino /", "Tastierino *",
    "Tastierino -", "Tab", "Q", "W",
    "E", "R", "T", "Y",
    "U", "I", "O", "P",
    "[", "]", "\\", "Canc (Del)",
    "Fine (End)", "Pag giù (PgDn)", "Tastierino 7", "Tastierino 8",
    "Tastierino 9", "Tastierino +", "Bloc Maiusc (Caps)", "A",
    "S", "D", "F", "G",
    "H", "J", "K", "L",
    ";", "'", "Invio (Enter)", "Tastierino 4",
    "Tastierino 5", "Tastierino 6", "Shift sinistro", "Z",
    "X", "C", "V", "B",
    "N", "M", ", (virgola)", ". (punto)",
    "/", "Shift destro", "Tastierino 1", "Tastierino 2",
    "Tastierino 3", "Tastierino 0", "Tastierino . (punto)", "Ctrl sinistro",
    "Win sinistro", "Alt sinistro", "Barra spaziatrice", "Alt destro",
    "Win destro", "Fn", "Ctrl destro", "Freccia sinistra",
    "Freccia su", "Freccia destra", "Freccia giù", "Tastierino Invio"
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
    {  95,  96,  97,  98,  99, 100, 101, 102, 103, 106, 105,  93,  94,   NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA,   NA }
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