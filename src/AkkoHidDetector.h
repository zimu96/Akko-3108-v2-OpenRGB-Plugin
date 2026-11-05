/*---------------------------------------------------------*\
| AkkoHidDetector.h                                          |
|                                                           |
|   HID device detection for the Akko 3108 V2 keyboard.      |
|                                                           |
|   The plugin API v5 has NO RegisterDeviceDetector hook:    |
|   detection is performed by the plugin itself with         |
|   hid_enumerate + hid_open_path, and the resulting        |
|   controller is registered as a virtual RGB controller     |
|   (CreateVirtualRGBController + RegisterVirtualRGBController,|
|   both part of OpenRGBPluginAPIInterface).                 |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#pragma once

#include <hidapi.h>
#include <string>
#include <vector>
#include "OpenRGBPluginInterface.h"

#define AKKO_3108_V2_VID                0x0C45
#define AKKO_3108_V2_PID                0x762B

/* Same interface/usage page used by the official core
   detector (EVision keyboard family). */
#define AKKO_KEYBOARD_USAGE_PAGE        0xFF1C
#define AKKO_KEYBOARD_INTERFACE         1

/*---------------------------------------------------------*\
| Logging (levels match OpenRGB LogManager:                 |
|   LL_ERROR=1, LL_WARNING=2, LL_INFO=3, LL_DEBUG=5)       |
| Writes to the OpenRGB log AND to the terminal.            |
\*---------------------------------------------------------*/
#define AKKO_LOG_ERROR   1
#define AKKO_LOG_WARNING 2
#define AKKO_LOG_INFO    3
#define AKKO_LOG_DEBUG   5

void AkkoLog(OpenRGBPluginAPIInterface* api, unsigned int level,
             const char* fmt, ...);

/*---------------------------------------------------------*\
| Describes one HID interface of the keyboard               |
\*---------------------------------------------------------*/
struct AkkoDeviceInfo
{
    std::string  path;                 /* hid_open_path() target          */
    int          interface_number;     /* USB interface number            */
    unsigned int usage_page;           /* usage page of the HID interface */
    unsigned int usage;                /* usage of the HID interface      */
    std::string  serial;               /* serial string (may be empty)    */
    std::string  product;              /* product string                  */
    std::string  manufacturer;         /* manufacturer string             */
};

/*---------------------------------------------------------*\
| Detection API (implementation in AkkoHidDetector.cpp)     |
\*---------------------------------------------------------*/

/* Enumerate EVERY HID device on the system and log VID/PID/
   interface/usage_page at debug level. Returns true if at
   least one Akko 3108 V2 interface was found. */
bool AkkoLogAllHidDevices(OpenRGBPluginAPIInterface* api);

/* Enumerate all interfaces of the Akko 3108 V2 and return
   them sorted by preference (exact match first). */
std::vector<AkkoDeviceInfo> AkkoEnumerateKeyboard(OpenRGBPluginAPIInterface* api);

/* All-enumeration → filter only the Akko 3108 V2 entries. */
std::vector<AkkoDeviceInfo> AkkoEnumerateKeyboardFromAll(hid_device_info* all_devs);

/* Scan for the keyboard, open the best interface and register
   a virtual RGB controller with the OpenRGB plugin API.
   On success returns the registered controller and (optionally)
   stores the ownership of the device bridge in *bridge_out (the
   caller must delete it when the controller is deleted, since the
   core's DeleteVirtualRGBController only frees the controller).
   Returns nullptr on failure. */
RGBControllerInterface* AkkoDetectAndRegister(OpenRGBPluginAPIInterface* api,
                                              class AkkoDeviceBridge** bridge_out = nullptr);