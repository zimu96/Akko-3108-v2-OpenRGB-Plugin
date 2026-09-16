#============================================================================#
# Akko-OpenRGB-Plugin                                                         #
#                                                                            #
#   Plugin OpenRGB 1.0 (Plugin API v5) che aggiunge un tab con la            #
#   tastiera Akko 3108 V2 disegnata e un editor per-tasto visuale.           #
#                                                                            #
#   Il plugin include un driver HID autonomo: quando caricato,               #
#   esegue una scansione hid_enumerate (il plugin API v5 NON dispone di     #
#   un hook RegisterDeviceDetector) e registra la tastiera come              #
#   controller RGB virtuale. Il bridge translate i colori in pacchetti       #
#   EVision V1 (EVision LED protocol).                                       #
#                                                                            #
#   Compila in modo AUTONOMO: l'SDK del plugin viene venduto nella           #
#   cartella sdk/ (nessuna dipendenza dai sorgenti completi di OpenRGB).     #
#                                                                            #
#   Build:                                                                    #
#     mkdir build && cd build                                                #
#     cmake ..                                                               #
#     make -j$(nproc)                                                        #
#                                                                            #
#   Per forzare Qt5 (OpenRGB 1.0 compilato con Qt5):                         #
#     cmake .. -DOPENRGB_PLUGIN_QT6=OFF                                      #
#                                                                            #
#   SPDX-License-Identifier: GPL-2.0-or-later                                #
#============================================================================#

cmake_minimum_required(VERSION 3.16)

project(AkkoKeyboardPlugin
        VERSION 2.0.0
        DESCRIPTION "OpenRGB plugin - Akko 3108 V2 driver + per-key editor"
        LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#----------------------------------------------------------------------------#
# AUTOMOC gestisce Q_OBJECT e Q_PLUGIN_METADATA (deve vedere metadata.json    #
# nella stessa cartella del file .h che dichiara Q_PLUGIN_METADATA).         #
#----------------------------------------------------------------------------#
set(CMAKE_AUTOMOC ON)

#----------------------------------------------------------------------------#
# Qt: OpenRGB 1.0 distribuito su Arch/CachyOS (AUR) usa Qt6.                 #
# Qt5 e' usato da build personalizzate della release 1.0.                    #
#----------------------------------------------------------------------------#
option(OPENRGB_PLUGIN_QT6
       "Compila contro Qt6 (OpenRGB di sistema Arch/CachyOS). OFF = Qt5."
       ON)

if(OPENRGB_PLUGIN_QT6)
    find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Gui Widgets)
else()
    find_package(QT NAMES Qt5 REQUIRED COMPONENTS Core Gui Widgets)
endif()

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

#----------------------------------------------------------------------------#
# hidapi (per hid_enumerate / hid_open_path nel driver plugin)                #
#                                                                            #
#  - Linux:   usa l'hidapi di sistema (pkg-config: hidapi-hidraw,            #
#             fallback hidapi-libusb).                                       #
#  - Windows: pkg-config non esiste: compila staticamente il backend         #
#             hidapi venduto in sdk/hidapi/ (BSD-3-Clause): il .dll del      #
#             plugin non richiede alcuna DLL hidapi esterna a runtime.       #
#----------------------------------------------------------------------------#
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HIDAPI QUIET hidapi-hidraw)
    if(NOT HIDAPI_FOUND)
        pkg_check_modules(HIDAPI QUIET hidapi-libusb)
    endif()
endif()

if(NOT HIDAPI_FOUND)
    if(WIN32)
        set(HIDAPI_VENDORED ON)
        add_library(hidapi_vendored STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/sdk/hidapi/hid_windows.c)
        target_include_directories(hidapi_vendored PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/sdk/hidapi)
        # Build statica: evita i decorator dllexport/dllimport.
        target_compile_definitions(hidapi_vendored PUBLIC HIDAPI_STATIC)
        # hidapi (backend Windows) usa SetupAPI per enumerare i device HID.
        target_link_libraries(hidapi_vendored PUBLIC setupapi)
        set(HIDAPI_LIBRARIES hidapi_vendored)
    else()
        message(FATAL_ERROR
            "hidapi non trovato. Installalo: Arch/CachyOS 'pacman -S hidapi', "
            "Debian/Ubuntu 'libhidapi-dev', Fedora 'hidapi-devel'.")
    endif()
endif()

#----------------------------------------------------------------------------#
# Libreria plugin (MODULE = plugin caricabile con QPluginLoader)             #
#----------------------------------------------------------------------------#
add_library(AkkoKeyboardPlugin MODULE
    src/AkkoKeyboardPlugin.cpp
    src/AkkoDeviceBridge.cpp
    src/AkkoHidDetector.cpp
    src/KeyboardCanvas.cpp
    src/KeyboardEditorWidget.cpp
)

target_include_directories(AkkoKeyboardPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk
    ${HIDAPI_INCLUDE_DIRS}
)

target_link_libraries(AkkoKeyboardPlugin PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    ${HIDAPI_LIBRARIES}
)

target_link_directories(AkkoKeyboardPlugin PRIVATE
    ${HIDAPI_LIBRARY_DIRS}
)

set_target_properties(AkkoKeyboardPlugin PROPERTIES
    OUTPUT_NAME AkkoKeyboardPlugin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)