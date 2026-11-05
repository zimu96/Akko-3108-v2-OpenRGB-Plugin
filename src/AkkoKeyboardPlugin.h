/*---------------------------------------------------------*\
| AkkoKeyboardPlugin.h                                      |
|                                                           |
|   OpenRGB plugin entry point for the Akko 3108 V2.        |
|                                                           |
|   The plugin ships TWO pieces:                            |
|     1. A bundled HID driver: at Load() it runs its own    |
|        hid_enumerate scan (the plugin API v5 has no       |
|        RegisterDeviceDetector hook) and registers the     |
|        keyboard as a virtual RGB controller whose         |
|        DeviceUpdate* callbacks write EVision V1 packets   |
|        over USB HID. This makes the Akko work in ANY      |
|        OpenRGB 1.0 build, driver or not.                  |
|     2. The visual per-key editor tab.                     |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#pragma once

#include <QObject>
#include <QImage>
#include <QMenu>
#include <thread>
#include <mutex>
#include <atomic>

#include "OpenRGBPluginInterface.h"

class KeyboardEditorWidget;
class AkkoDeviceBridge;

class AkkoKeyboardPlugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "metadata.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    AkkoKeyboardPlugin();
    ~AkkoKeyboardPlugin() override;

    /*-----------------------------------------------------*\
    | OpenRGBPluginInterface                                 |
    \*-----------------------------------------------------*/
    OpenRGBPluginInfo           GetPluginInfo() override;
    unsigned int                GetPluginAPIVersion() override;
    void                        Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*                    GetWidget() override;
    QMenu*                      GetTrayMenu() override;
    void                        Unload() override;
    void                        OnProfileAboutToLoad() override;
    void                        OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json              OnProfileSave() override;
    unsigned char*              OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;
    void                        ProfileManagerUpdated(unsigned int update_reason) override;
    void                        ResourceManagerUpdated(unsigned int update_reason) override;
    void                        SettingsManagerUpdated(unsigned int update_reason) override;

private:
    void                        RunDetection();
    void                        StopDetection();

    OpenRGBPluginAPIInterface*  api_ptr;
    KeyboardEditorWidget*       editor_widget;

    RGBControllerInterface*     virtual_controller;  /* device registered by the plugin */
    AkkoDeviceBridge*           device_bridge;       /* HID bridge owned by the plugin  */
    std::thread                 detection_thread;
    std::mutex                  detect_mutex;
    std::atomic<bool>           detection_running;
    std::atomic<bool>           stop_requested;
};