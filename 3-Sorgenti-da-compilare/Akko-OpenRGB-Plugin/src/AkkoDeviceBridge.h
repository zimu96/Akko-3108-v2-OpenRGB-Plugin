/*---------------------------------------------------------*\
| AkkoDeviceBridge.h                                        |
|                                                           |
|   Plugin-side HID driver for the Akko 3108 V2 keyboard.   |
|                                                           |
|   Translates OpenRGB color/mode updates into EVision V1   |
|   USB HID packets (VID 0x0C45 PID 0x762B).               |
|                                                           |
|   Used as the DeviceUpdate* callback target in the        |
|   RGBController_Setup that the plugin registers via       |
|   OpenRGBPluginAPIInterface::CreateVirtualRGBController.  |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#pragma once

#include <hidapi.h>
#include <mutex>
#include <string>
#include <vector>
#include "RGBControllerInterface.h"

/*---------------------------------------------------------*\
| Protocol constants (from reverse engineering, same as     |
| AkkoKeyboardController.h in the custom driver build)     |
\*---------------------------------------------------------*/
#define AKKO_BRIDGE_REPORT_ID          0x04
#define AKKO_BRIDGE_MAX_PACKET_SIZE    0x36    /* 54 bytes = 18 LEDs */
#define AKKO_BRIDGE_NUM_LEDS           108     /* visible keys       */
#define AKKO_BRIDGE_FIRMWARE_SLOTS     133     /* firmware LED slots */

/* HID packet command bytes */
#define AKKO_BRIDGE_COMMAND_BEGIN                   0x01
#define AKKO_BRIDGE_COMMAND_END                     0x02
#define AKKO_BRIDGE_COMMAND_SET_PARAMETER           0x06
#define AKKO_BRIDGE_COMMAND_WRITE_CUSTOM_COLOR_DATA 0x11

/* Parameter IDs */
#define AKKO_BRIDGE_PARAMETER_MODE                  0x00
#define AKKO_BRIDGE_PARAMETER_BRIGHTNESS            0x01
#define AKKO_BRIDGE_PARAMETER_SPEED                 0x02
#define AKKO_BRIDGE_PARAMETER_DIRECTION             0x03
#define AKKO_BRIDGE_PARAMETER_RANDOM_COLOR_FLAG     0x04
#define AKKO_BRIDGE_PARAMETER_MODE_COLOR            0x05

/* Mode firmware values */
enum AkkoBridgeMode
{
    AKKO_BRIDGE_MODE_OFF                = 0x00,
    AKKO_BRIDGE_MODE_COLOR_WAVE_SHORT   = 0x01,
    AKKO_BRIDGE_MODE_COLOR_WAVE_LONG    = 0x02,
    AKKO_BRIDGE_MODE_COLOR_WHEEL        = 0x03,
    AKKO_BRIDGE_MODE_SPECTRUM_CYCLE     = 0x04,
    AKKO_BRIDGE_MODE_BREATHING          = 0x05,
    AKKO_BRIDGE_MODE_STATIC             = 0x06,
    AKKO_BRIDGE_MODE_REACTIVE           = 0x07,
    AKKO_BRIDGE_MODE_REACTIVE_RIPPLE    = 0x08,
    AKKO_BRIDGE_MODE_REACTIVE_LINE      = 0x09,
    AKKO_BRIDGE_MODE_STARLIGHT_FAST     = 0x0A,
    AKKO_BRIDGE_MODE_BLOOMING           = 0x0B,
    AKKO_BRIDGE_MODE_RAINBOW_WAVE_VERT  = 0x0C,
    AKKO_BRIDGE_MODE_HURRICANE          = 0x0D,
    AKKO_BRIDGE_MODE_ACCUMULATE         = 0x0E,
    AKKO_BRIDGE_MODE_STARLIGHT_SLOW     = 0x0F,
    AKKO_BRIDGE_MODE_VISOR              = 0x10,
    AKKO_BRIDGE_MODE_SURMOUNT           = 0x11,
    AKKO_BRIDGE_MODE_RAINBOW_WAVE_CIRC  = 0x12,
    AKKO_BRIDGE_MODE_CUSTOM             = 0x14
};

/* Brightness / speed range */
#define AKKO_BRIDGE_BRIGHTNESS_LOWEST  0x00
#define AKKO_BRIDGE_BRIGHTNESS_HIGHEST 0x04
#define AKKO_BRIDGE_SPEED_SLOWEST      0x05
#define AKKO_BRIDGE_SPEED_NORMAL       0x03
#define AKKO_BRIDGE_SPEED_FASTEST      0x00

/*---------------------------------------------------------*\
| AkkoDeviceBridge                                           |
|                                                           |
|   Manages a single HID connection to the keyboard and     |
|   provides the static callback functions for              |
|   RGBController_Setup function pointers.                  |
|                                                           |
|   object_ptr in RGBController_Setup points to the bridge. |
|   The bridge holds:                                        |
|     - hid_device* (the keyboard's LED interface handle)   |
|     - RGBControllerInterface* (set after registration)    |
|     - cached mode values (needed to build protocol pkts)  |
\*---------------------------------------------------------*/
class AkkoDeviceBridge
{
public:
    explicit AkkoDeviceBridge(hid_device* dev, const std::string& hid_path);
    ~AkkoDeviceBridge();

    void SetController(RGBControllerInterface* controller);
    void SetModeValues(const std::vector<int>& values);

    /*-----------------------------------------------------*\
    | Static callback functions for RGBController_Setup     |
    \*-----------------------------------------------------*/
    static void DeviceUpdateLEDs(void* obj);
    static void DeviceUpdateZoneLEDs(void* obj, int zone);
    static void DeviceUpdateSingleLED(void* obj, int led);
    static void DeviceUpdateMode(void* obj);
    static void DeviceUpdateZoneMode(void* obj, int zone);
    static void DeviceSaveMode(void* obj);
    static void DeviceConfigureZone(void* obj, int zone);

private:
    void SendAndRead(unsigned char* buf);
    void ComputeChecksum(char* buf);
    void SendKeyboardBegin();
    void SendKeyboardEnd();
    void SendKeyboardParameter(unsigned char param, unsigned char param_size,
                               unsigned char* data);
    void SendKeyboardColorData(unsigned char* data, unsigned int size,
                               unsigned int offset);
    void SendKeyboardMode(unsigned char mode, unsigned char brightness,
                          unsigned char speed, unsigned char direction,
                          unsigned char random_flag, unsigned char red,
                          unsigned char green, unsigned char blue);
    void SetKeyboardColors(unsigned char* data, unsigned int size);
    void SendKeyboardCustomColors(unsigned char* color_data,
                                  unsigned int num_leds,
                                  unsigned char brightness);

    /* Read current mode params from the RGBControllerInterface */
    void ReadModeParams(unsigned char& mode_val, unsigned char& brightness,
                        unsigned char& speed, unsigned char& direction,
                        unsigned char& random_flag, unsigned char& red,
                        unsigned char& green, unsigned char& blue);

    hid_device*             dev;
    std::string             hid_path;
    RGBControllerInterface* controller;
    std::mutex              write_mutex;

    /* Cached mode value for each mode index (populated at registration) */
    std::vector<int>        mode_values;
};
