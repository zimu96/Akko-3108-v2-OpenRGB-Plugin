/*---------------------------------------------------------*\
| AkkoKeyboardPlugin.cpp                                    |
|                                                           |
|   OpenRGB plugin entry point for the Akko 3108 V2.        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later              |
\*---------------------------------------------------------*/
#include "AkkoKeyboardPlugin.h"
#include "KeyboardEditorWidget.h"
#include "AkkoHidDetector.h"
#include "AkkoDeviceBridge.h"

#include <hidapi.h>
#include <cstdio>

AkkoKeyboardPlugin::AkkoKeyboardPlugin() :
    QObject(),
    OpenRGBPluginInterface(),
    api_ptr(nullptr),
    editor_widget(nullptr),
    virtual_controller(nullptr),
    device_bridge(nullptr),
    detection_running(false),
    stop_requested(false)
{
}

AkkoKeyboardPlugin::~AkkoKeyboardPlugin()
{
    /* The OpenRGB dialog owns the widget once GetWidget()   */
    /* is called; nothing else to free here.                */
    StopDetection();
}

OpenRGBPluginInfo AkkoKeyboardPlugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name                   = "Akko 3108 V2 Editor";
    info.Description            = "Driver HID + editor per-tasto visuale per la tastiera "
                                  "Akko 3108 V2 (Keyboard H / H3108 V2, VID 0x0C45 PID 0x762B). "
                                  "Rileva la tastiera via hid_enumerate e la registra in OpenRGB; "
                                  "disegna la tastiera e consente di colorarla per-tasto.";
    info.Version                = "2.0";
    info.Commit                 = "";
    info.URL                    = "";
    info.Icon                   = QImage();
    info.Location               = OPENRGB_PLUGIN_LOCATION_TOP;
    info.Label                  = "Akko Editor";
    info.TabIconString          = "";
    info.TabIcon                = QImage();
    info.ProtocolVersion        = 5;

    return(info);
}

unsigned int AkkoKeyboardPlugin::GetPluginAPIVersion()
{
    return(OPENRGB_PLUGIN_API_VERSION);
}

/*---------------------------------------------------------*\
| Load: start the HID detection thread                      |
|                                                           |
| The plugin API v5 has no RegisterDeviceDetector hook, so  |
| the plugin performs its own hid_enumerate scan here (and  |
| again on RescanDevices / device list updates while the    |
| keyboard is still missing).                               |
\*---------------------------------------------------------*/
void AkkoKeyboardPlugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api_ptr = plugin_api_ptr;

    /* Consente un eventuale secondo ciclo load/unload */
    stop_requested.store(false);

    RunDetection();
}

QWidget* AkkoKeyboardPlugin::GetWidget()
{
    if(editor_widget == nullptr)
    {
        editor_widget = new KeyboardEditorWidget(api_ptr);
    }

    return(editor_widget);
}

QMenu* AkkoKeyboardPlugin::GetTrayMenu()
{
    return(nullptr);
}

void AkkoKeyboardPlugin::Unload()
{
    StopDetection();

    std::lock_guard<std::mutex> lock(detect_mutex);

    if((api_ptr != nullptr) && (virtual_controller != nullptr))
    {
        api_ptr->UnregisterVirtualRGBController(virtual_controller);
        api_ptr->DeleteVirtualRGBController(virtual_controller);
        virtual_controller = nullptr;
    }

    /* Il bridge (object_ptr del controller) è di proprietà del
       plugin: il core elimina solo il controller. */
    if(device_bridge != nullptr)
    {
        delete device_bridge;
        device_bridge = nullptr;
    }

    api_ptr = nullptr;
}

void AkkoKeyboardPlugin::OnProfileAboutToLoad()
{
}

void AkkoKeyboardPlugin::OnProfileLoad(nlohmann::json /*profile_data*/)
{
}

nlohmann::json AkkoKeyboardPlugin::OnProfileSave()
{
    nlohmann::json profile_json;

    return(profile_json);
}

unsigned char* AkkoKeyboardPlugin::OnSDKCommand(unsigned int /*pkt_id*/, unsigned char* /*pkt_data*/, unsigned int* /*pkt_size*/)
{
    return(nullptr);
}

void AkkoKeyboardPlugin::ProfileManagerUpdated(unsigned int /*update_reason*/)
{
}

/*---------------------------------------------------------*\
| Resource manager updated                                   |
|                                                           |
| Re-runs detection when the device list changes and the    |
| keyboard is not registered yet (covers "Rescan Devices"   |
| and plugging the keyboard in after OpenRGB startup).      |
\*---------------------------------------------------------*/
void AkkoKeyboardPlugin::ResourceManagerUpdated(unsigned int /*update_reason*/)
{
    if(api_ptr == nullptr)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(detect_mutex);

        if((virtual_controller != nullptr) || detection_running.load())
        {
            /* Already registered or a scan is in progress */
            return;
        }
    }

    AkkoLog(api_ptr, AKKO_LOG_DEBUG,
            "Device list updated: re-running Akko detection");

    RunDetection();
}

void AkkoKeyboardPlugin::SettingsManagerUpdated(unsigned int /*update_reason*/)
{
}

/*---------------------------------------------------------*\
| Detection                                                 |
\*---------------------------------------------------------*/
void AkkoKeyboardPlugin::RunDetection()
{
    std::lock_guard<std::mutex> lock(detect_mutex);

    if(detection_running.load() || stop_requested.load() || (api_ptr == nullptr))
    {
        return;
    }

    /* Join the previous scan if it already finished (it is only
       reachable here when detection_running == false, i.e. the
       previous thread has completed its work). */
    if(detection_thread.joinable())
    {
        detection_thread.join();
    }

    detection_running.store(true);

    OpenRGBPluginAPIInterface* api = api_ptr;

    detection_thread = std::thread([this, api]()
    {
        /* hidapi init (refcounted) */
        hid_init();

        AkkoDeviceBridge* bridge = nullptr;
        RGBControllerInterface* rgb = AkkoDetectAndRegister(api, &bridge);

        {
            std::lock_guard<std::mutex> lock2(detect_mutex);

            if(rgb != nullptr)
            {
                virtual_controller = rgb;
                device_bridge      = bridge;
            }

            detection_running.store(false);
        }

        if(!stop_requested.load())
        {
            /* balance hid_init() once the scan is done */
            hid_exit();
        }
    });
}

void AkkoKeyboardPlugin::StopDetection()
{
    stop_requested.store(true);

    if(detection_thread.joinable())
    {
        detection_thread.join();
    }
}