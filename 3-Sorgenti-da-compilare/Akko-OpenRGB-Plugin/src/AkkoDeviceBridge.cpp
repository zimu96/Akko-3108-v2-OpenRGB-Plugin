/*---------------------------------------------------------*\
| AkkoDeviceBridge.cpp                                      |
|                                                           |
|   Plugin-side HID driver implementation for the Akko      |
|   3108 V2 keyboard (EVision V1 protocol).                 |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#include "AkkoDeviceBridge.h"

#include <cstring>
#include <thread>
#include <chrono>

/*---------------------------------------------------------*\
| LED index -> firmware slot map                            |
|                                                           |
| LED index 0-107 = the OpenRGB zone order (keys sorted by  |
| firmware slot). Translates to the 133-slot EVision V1     |
| firmware buffer. Confirmed on hardware (same table used   |
| by the custom driver build).                              |
\*---------------------------------------------------------*/
static const unsigned char led_slot_map[AKKO_BRIDGE_NUM_LEDS] =
{
    /*   0-12    Esc + F1-F12                                   */
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    /*  13-20    PrtSc ScrLk Pause Calc Vol- Vol+ Mute `        */
    14, 15, 16, 17, 18, 19, 20, 21,
    /*  21-36    1-0 - = Back Ins Home PgUp                     */
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
    /*  37-40    NumLk Num/ Num* Num-                           */
    38, 39, 40, 41,
    /*  41-57    Tab Q-P [ ] \ Del End PgDn                     */
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58,
    /*  58-61    Num7 Num8 Num9 Num+                            */
    59, 60, 61, 62,
    /*  62-73    Caps A-L ; '                                   */
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
    /*  74       Enter                                          */
    76,
    /*  75-77    Num4 Num5 Num6                                 */
    80, 81, 82,
    /*  78-88    LShift Z-M , . /                               */
    84, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    /*  89       RShift                                         */
    97,
    /*  90-103   Num1 Num2 Num3 Num0 Num. LCtrl Win LAlt Space  */
    /*           RAlt Fn RCtrl Menu Left                        */
    99, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
    /* 104-106   Up Right Down                                  */
    119, 120, 121,
    /* 107       NumEnter                                       */
    123
};

/*---------------------------------------------------------*\
| Constructor / destructor                                  |
\*---------------------------------------------------------*/
AkkoDeviceBridge::AkkoDeviceBridge(hid_device* dev_ptr, const std::string& path)
    : dev(dev_ptr)
    , hid_path(path)
    , controller(nullptr)
{
}

AkkoDeviceBridge::~AkkoDeviceBridge()
{
    if(dev != nullptr)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

void AkkoDeviceBridge::SetController(RGBControllerInterface* rgb_controller)
{
    controller = rgb_controller;
}

void AkkoDeviceBridge::SetModeValues(const std::vector<int>& values)
{
    mode_values = values;
}

/*---------------------------------------------------------*\
| HID low level                                             |
\*---------------------------------------------------------*/
void AkkoDeviceBridge::SendAndRead(unsigned char* buf)
{
    ComputeChecksum((char*)buf);

    if(dev != nullptr)
    {
        hid_write(dev, buf, 64);
    }

    /* Best-effort echo read with timeout (device echoes most
       packets; never block forever). */
    unsigned char resp[64];

    for(int retry = 0; retry < 20; retry++)
    {
        int ret = hid_read(dev, resp, 64);

        if(ret > 0)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(3));
    }
}

void AkkoDeviceBridge::ComputeChecksum(char* buf)
{
    unsigned short checksum = 0;

    for(unsigned int byte_idx = 0x03; byte_idx < 64; byte_idx++)
    {
        checksum += (unsigned char)buf[byte_idx];
    }

    buf[0x01] = checksum & 0xFF;
    buf[0x02] = checksum >> 8;
}

/*---------------------------------------------------------*\
| Packet builders                                           |
\*---------------------------------------------------------*/
void AkkoDeviceBridge::SendKeyboardBegin()
{
    unsigned char buf[64];

    memset(buf, 0x00, sizeof(buf));

    buf[0x00] = AKKO_BRIDGE_REPORT_ID;
    buf[0x03] = AKKO_BRIDGE_COMMAND_BEGIN;

    SendAndRead(buf);
}

void AkkoDeviceBridge::SendKeyboardEnd()
{
    unsigned char buf[64];

    memset(buf, 0x00, sizeof(buf));

    buf[0x00] = AKKO_BRIDGE_REPORT_ID;
    buf[0x03] = AKKO_BRIDGE_COMMAND_END;

    SendAndRead(buf);
}

void AkkoDeviceBridge::SendKeyboardParameter(unsigned char parameter,
                                             unsigned char parameter_size,
                                             unsigned char* parameter_data)
{
    unsigned char buf[64];

    memset(buf, 0x00, sizeof(buf));

    buf[0x00] = AKKO_BRIDGE_REPORT_ID;
    buf[0x03] = AKKO_BRIDGE_COMMAND_SET_PARAMETER;
    buf[0x04] = parameter_size;
    buf[0x05] = parameter;

    memcpy(&buf[0x08], parameter_data, parameter_size);

    SendAndRead(buf);
}

void AkkoDeviceBridge::SendKeyboardColorData(unsigned char* data,
                                             unsigned int size,
                                             unsigned int offset)
{
    unsigned char buf[64];

    memset(buf, 0x00, sizeof(buf));

    buf[0x00] = AKKO_BRIDGE_REPORT_ID;
    buf[0x03] = AKKO_BRIDGE_COMMAND_WRITE_CUSTOM_COLOR_DATA;
    buf[0x04] = size;
    buf[0x05] = offset & 0xFF;
    buf[0x06] = (offset >> 8) & 0xFF;

    memcpy(&buf[0x08], data, size);

    SendAndRead(buf);
}

/*---------------------------------------------------------*\
| Mode / colors transactions                                |
|                                                           |
| Verified sequence: BEGIN + 6x SET_PARAM (or color data    |
| chunks) + END. Without BEGIN/END some effects are flaky.  |
\*---------------------------------------------------------*/
void AkkoDeviceBridge::SendKeyboardMode(unsigned char mode,
                                        unsigned char brightness,
                                        unsigned char speed,
                                        unsigned char direction,
                                        unsigned char random_flag,
                                        unsigned char red,
                                        unsigned char green,
                                        unsigned char blue)
{
    unsigned char param[3];

    SendKeyboardBegin();

    param[0] = mode;         SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_MODE,             1, param);
    param[0] = brightness;   SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_BRIGHTNESS,       1, param);
    param[0] = speed;        SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_SPEED,            1, param);
    param[0] = direction;    SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_DIRECTION,        1, param);
    param[0] = random_flag;  SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_RANDOM_COLOR_FLAG,1, param);
    param[0] = red;
    param[1] = green;
    param[2] = blue;         SendKeyboardParameter(AKKO_BRIDGE_PARAMETER_MODE_COLOR,       3, param);

    SendKeyboardEnd();
}

void AkkoDeviceBridge::SetKeyboardColors(unsigned char* data, unsigned int size)
{
    unsigned int packet_size   = 0;
    unsigned int packet_offset = 0;

    SendKeyboardBegin();

    while(size > 0)
    {
        if(size >= AKKO_BRIDGE_MAX_PACKET_SIZE)
        {
            packet_size = AKKO_BRIDGE_MAX_PACKET_SIZE;
        }
        else
        {
            packet_size = size;
        }

        SendKeyboardColorData(&data[packet_offset], packet_size, packet_offset);

        size          -= packet_size;
        packet_offset += packet_size;
    }

    SendKeyboardEnd();
}

void AkkoDeviceBridge::SendKeyboardCustomColors(unsigned char* color_data,
                                                unsigned int num_leds,
                                                unsigned char brightness)
{
    unsigned char fw_buffer[3 * AKKO_BRIDGE_FIRMWARE_SLOTS];

    /* Build the full 133-slot firmware buffer; empty slots stay black */
    memset(fw_buffer, 0x00, sizeof(fw_buffer));

    for(unsigned int led_idx = 0;
        led_idx < num_leds && led_idx < AKKO_BRIDGE_NUM_LEDS;
        led_idx++)
    {
        unsigned int slot = led_slot_map[led_idx];

        fw_buffer[(3 * slot) + 0] = color_data[(3 * led_idx) + 0];
        fw_buffer[(3 * slot) + 1] = color_data[(3 * led_idx) + 1];
        fw_buffer[(3 * slot) + 2] = color_data[(3 * led_idx) + 2];
    }

    /* MODE_CUSTOM transaction + full color write */
    SendKeyboardMode(AKKO_BRIDGE_MODE_CUSTOM,
                     brightness,
                     AKKO_BRIDGE_SPEED_NORMAL,
                     0x01,           /* direction: right */
                     0x00,           /* random: off      */
                     0x00, 0x00, 0x00);

    SetKeyboardColors(fw_buffer, sizeof(fw_buffer));
}

/*---------------------------------------------------------*\
| Read current mode parameters from the RGBController        |
| (called from the DeviceUpdate* callbacks).                 |
\*---------------------------------------------------------*/
void AkkoDeviceBridge::ReadModeParams(unsigned char& mode_val,
                                      unsigned char& brightness,
                                      unsigned char& speed,
                                      unsigned char& direction,
                                      unsigned char& random_flag,
                                      unsigned char& red,
                                      unsigned char& green,
                                      unsigned char& blue)
{
    mode_val     = AKKO_BRIDGE_MODE_CUSTOM;
    brightness   = AKKO_BRIDGE_BRIGHTNESS_HIGHEST;
    speed        = AKKO_BRIDGE_SPEED_NORMAL;
    direction    = 0x01;
    random_flag  = 0x00;
    red = green = blue = 0x00;

    if(controller == nullptr)
    {
        return;
    }

    int active = controller->GetActiveMode();

    if((active < 0) || (active >= (int)mode_values.size()))
    {
        return;
    }

    mode_val = (unsigned char)mode_values[(size_t)active];

    brightness = (unsigned char)controller->GetModeBrightness((unsigned int)active);
    speed      = (unsigned char)controller->GetModeSpeed((unsigned int)active);
    direction  = (unsigned char)controller->GetModeDirection((unsigned int)active);

    if(controller->GetModeColorMode((unsigned int)active) == MODE_COLORS_RANDOM)
    {
        random_flag = 0x01;
    }

    if(controller->GetModeColorsCount((unsigned int)active) > 0)
    {
        RGBColor color = controller->GetModeColor((unsigned int)active, 0);
        red   = RGBGetRValue(color);
        green = RGBGetGValue(color);
        blue  = RGBGetBValue(color);
    }
}

/*---------------------------------------------------------*\
| Static callbacks (RGBController_Setup function pointers)   |
\*---------------------------------------------------------*/
void AkkoDeviceBridge::DeviceUpdateLEDs(void* obj)
{
    AkkoDeviceBridge* bridge = static_cast<AkkoDeviceBridge*>(obj);

    if(bridge == nullptr || bridge->dev == nullptr)
    {
        return;
    }

    unsigned char mode_val = 0, brightness = 0, speed = 0;
    unsigned char direction = 0, random_flag = 0, red = 0, green = 0, blue = 0;

    bridge->ReadModeParams(mode_val, brightness, speed,
                           direction, random_flag, red, green, blue);

    if(mode_val != AKKO_BRIDGE_MODE_CUSTOM)
    {
        /* Colors only apply in CUSTOM mode: re-send the current
           mode (covers mode-color / brightness UI changes). */
        std::lock_guard<std::mutex> lock(bridge->write_mutex);
        bridge->SendKeyboardMode(mode_val, brightness, speed, direction,
                                 random_flag, red, green, blue);
        return;
    }

    if(bridge->controller == nullptr)
    {
        return;
    }

    /* Full custom color update from the OpenRGB color buffer */
    unsigned char color_data[3 * AKKO_BRIDGE_NUM_LEDS];

    RGBColor* colors = bridge->controller->GetColorsPointer();

    if(colors == nullptr)
    {
        return;
    }

    for(unsigned int led_idx = 0; led_idx < AKKO_BRIDGE_NUM_LEDS; led_idx++)
    {
        color_data[(3 * led_idx) + 0] = RGBGetRValue(colors[led_idx]);
        color_data[(3 * led_idx) + 1] = RGBGetGValue(colors[led_idx]);
        color_data[(3 * led_idx) + 2] = RGBGetBValue(colors[led_idx]);
    }

    std::lock_guard<std::mutex> lock(bridge->write_mutex);
    bridge->SendKeyboardCustomColors(color_data, AKKO_BRIDGE_NUM_LEDS, brightness);
}

void AkkoDeviceBridge::DeviceUpdateZoneLEDs(void* obj, int /*zone*/)
{
    DeviceUpdateLEDs(obj);
}

void AkkoDeviceBridge::DeviceUpdateSingleLED(void* obj, int /*led*/)
{
    DeviceUpdateLEDs(obj);
}

void AkkoDeviceBridge::DeviceUpdateMode(void* obj)
{
    AkkoDeviceBridge* bridge = static_cast<AkkoDeviceBridge*>(obj);

    if(bridge == nullptr || bridge->dev == nullptr)
    {
        return;
    }

    unsigned char mode_val = 0, brightness = 0, speed = 0;
    unsigned char direction = 0, random_flag = 0, red = 0, green = 0, blue = 0;

    bridge->ReadModeParams(mode_val, brightness, speed,
                           direction, random_flag, red, green, blue);

    if(mode_val == AKKO_BRIDGE_MODE_CUSTOM)
    {
        /* CUSTOM shows the LED buffer: send the colors now */
        DeviceUpdateLEDs(obj);
        return;
    }

    std::lock_guard<std::mutex> lock(bridge->write_mutex);
    bridge->SendKeyboardMode(mode_val, brightness, speed, direction,
                             random_flag, red, green, blue);
}

void AkkoDeviceBridge::DeviceUpdateZoneMode(void* obj, int /*zone*/)
{
    DeviceUpdateMode(obj);
}

void AkkoDeviceBridge::DeviceSaveMode(void* /*obj*/)
{
    /* The keyboard firmware auto-saves settings; nothing to do. */
}

void AkkoDeviceBridge::DeviceConfigureZone(void* /*obj*/, int /*zone*/)
{
    /* Not resizable: the matrix zone is fixed at 108 LEDs. */
}