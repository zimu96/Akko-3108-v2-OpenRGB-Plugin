# Akko-OpenRGB-Plugin

[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL--2.0--or--later-blue.svg)](LICENSE)

OpenRGB plugin (Plugin API v5) that adds an **"Akko Editor"** tab with a drawn,
realistic keyboard for the **Akko 3108 V2** (Keyboard H / H3108 V2, VID
`0x0C45` PID `0x762B`).

<img width="1303" height="793" alt="OpenRGB-akko-3108-V2-Akko-Editor" src="https://github.com/user-attachments/assets/594fbb74-0b16-4e17-beee-a3bdd1c5215e" />
<img width="1303" height="793" alt="OpenRGB-akko-3108-V2-Dispositivi" src="https://github.com/user-attachments/assets/5ba94345-7c44-42b3-8c4e-0a14ed8cd868" />
<img width="1303" height="793" alt="OpenRGB-akko-3108-V2-Impostazioni-Plugin" src="https://github.com/user-attachments/assets/184e3d2e-7613-4ea5-92dd-c922d2576987" />

Every key is rendered as a rounded rectangle at its real position and scale.
Click a key to set its LED color; a click again with the same color turns it
off.

Since v2.0 the plugin also contains a **full HID driver for the keyboard**:
it scans the USB bus itself (`hid_enumerate`), opens the LED interface
(`interface 1, usage page 0xFF1C`) and registers the device in OpenRGB as a
virtual RGB controller whose `DeviceUpdate*` callbacks write EVision V1
packets over HID. This makes the Akko work in **any** OpenRGB 1.0 build —
**including stock builds that do not ship the Akko driver**.

Built **fully standalone**: the OpenRGB Plugin SDK headers are vendored in
[`sdk/`](sdk/), so **no OpenRGB source tree is required** to compile. The only
runtime dependency is `hidapi` (already a dependency of OpenRGB).

---

## Features

- Top-level **"Akko Editor"** tab in OpenRGB (no core modifications).
- Keyboard drawn with real key proportions (layout generated from the
  keyboard's `MAIN.INI`).
- 10-color palette + custom color picker + "All black" reset.
- **Bundled device driver** (`AkkoHidDetector` + `AkkoDeviceBridge`):
  - scans at plugin load, re-scans on device-list updates / "Rescan Devices";
  - debug log of **every HID device** found (VID/PID/interface/usage page)
    and the outcome (success/failure) of every `hid_open` attempt;
  - tries the LED-control interface first (`interface 1`, `usage_page 0xFF1C`,
    same as the official EVision detector), then vendor page `0xFF00`, then any
    secondary interface;
  - registers a **virtual RGB controller** named "Akko 3108 V2" with the full
    19-mode setup and the 6×22 matrix (108 LEDs) — identical to the core
    driver;
  - per-key colors and effects are translated into EVision protocol packets
    (report `0x04`, BEGIN/END framing, checksum, 133-slot firmware map);
  - **auto-skips** registration when the OpenRGB core already detected an
    Akko (custom driver builds) to avoid duplicate devices.
- Automatic detection of the controller (`GetName()` contains `Akko` or
  `3108`) — works whether the device comes from the plugin driver or from the
  core.
- Configurable auto-refresh of LED colors from the OpenRGB buffer.

---

## Compatibility

| Environment                                 | Qt   | Plugin API | Status                                                |
|---------------------------------------------|------|------------|-------------------------------------------------------|
| OpenRGB 1.0 (Arch / CachyOS, AUR package)   | Qt6  | v5         | ✅ supported (bundled driver replaces the missing core driver) |
| OpenRGB 1.0 custom builds (compiled with Qt5) | Qt5 | v5         | ✅ supported (uses the core driver, plugin editor only) |
| OpenRGB 1.0 (Windows, official MSVC build)  | Qt6  | v5         | ✅ build via CI (`.dll`), not yet hardware-tested |

The plugin reports API version **5** (`OPENRGB_PLUGIN_API_VERSION`), matching
OpenRGB 1.0. Build against the **same Qt major version** as your OpenRGB
binary (the AUR `openrgb` package uses Qt6 — a plugin built with Qt5 will be
refused by a Qt6 host and shown as incompatible). On Windows the plugin must
also be built with the **same compiler** as the OpenRGB binary: the official
Windows builds use **MSVC**, so the `.dll` from the CI workflow is MSVC-built
(a MinGW/GCC `.dll` would not load).

### Why a bundled driver? (SDK plugin API v5)

The plugin API v5 does **not** expose the core's `RegisterDeviceDetector`
hook, so a plugin cannot register a *core detector*. The equivalent mechanism
offered by the SDK is:

1. `CreateVirtualRGBController(RGBController_Setup*)` — build a device
   description (name, modes, zones, LEDs, `object_ptr` and the
   `DeviceUpdateLEDs/DeviceUpdateMode/…` function pointers);
2. `RegisterVirtualRGBController(...)` — the core then adds it to the
   ResourceManager device list (`UpdateDeviceList`), exactly where a
   core-detected device would land.

The plugin therefore ships its own enumeration (`hid_enumerate`) + a device
bridge implementing the `DeviceUpdate*` callbacks with EVision HID packets.

---

## Requirements

- CMake ≥ 3.16, a C++17 compiler:
  - Linux: GCC/Clang + `make`
  - Windows: **Visual Studio 2022 (MSVC)** — required for ABI compatibility
    with the official Windows OpenRGB build
- Qt development packages:
  - `qt6-base` (default, for the AUR/system OpenRGB on Arch & CachyOS)
  - or `qt5-base` (`-DOPENRGB_PLUGIN_QT6=OFF`)
- `hidapi`:
  - Linux: development package + pkg-config (`hidapi-hidraw` — Arch:
    `sudo pacman -S hidapi`)
  - Windows: none — hidapi is vendored in `sdk/hidapi/` and compiled
    statically into the plugin
- `hidraw` device permission for your user (Arch default udev rules already
  grant access; otherwise run OpenRGB under a user in the `input` group)

`nlohmann/json` and the OpenRGB Plugin SDK headers are vendored inside this
repository.

---

## Build & Install (Linux — Arch / CachyOS)

```bash
cd Akko-OpenRGB-Plugin
mkdir build && cd build
cmake ..
make -j$(nproc)
```

Copy the freshly built library into the user plugin folder:

```bash
cp libAkkoKeyboardPlugin.so ~/.config/OpenRGB/plugins/
```

> If a previous build of the plugin exists in that folder, replace it (backup
> first if you rely on an old Qt5 build).

### Build against Qt5 (custom OpenRGB 1.0 builds)

```bash
cmake .. -DOPENRGB_PLUGIN_QT6=OFF
make -j$(nproc)
```

---

## Enabling the plugin

1. Launch OpenRGB (system package or your custom build) from a terminal to see
   the plugin's detection log, e.g.:
   ```bash
   openrgb --startminimized
   ```
2. `Settings → Plugins` (Impostazioni → Plugin).
3. The plugin entry is now **compatible** (no red "API version"): tick
   **Enabled** (Attivato).
4. On the next launch the plugin scans the HID bus: you should see debug lines
   like `HID device #N: VID=0x0C45 PID=0x762B …` and
   `hid_open SUCCESS on interface 1 (usage_page 0xFF1C)` in the log, then
   `Registered 'Akko 3108 V2' (interface 1, 19 modes, 108 LEDs)`.
5. The **"Akko Editor"** tab appears in the main tab bar. Open it: the
   keyboard is drawn; pick a color and click keys to light them. The keyboard
   also shows up in the **Devices** tab with all 19 effects.

---

## Distribution (GitHub Releases)

Two options are offered for every release:

1. **Prebuilt binaries** — attached to the GitHub Release as ZIPs, produced
   automatically by GitHub Actions ([`.github/workflows/release-builds.yml`](.github/workflows/release-builds.yml))
   on the *target* system (no cross-compilation → no ABI/glibc surprises):
   - `Akko-OpenRGB-Plugin-linux-x86_64.zip` — `.so` built on Ubuntu 22.04
     (glibc 2.35): works on basically every distro from mid-2022 on
     (Arch/CachyOS/Manjaro, Ubuntu 22.04+, Debian 12+, Fedora 36+…);
   - `Akko-OpenRGB-Plugin-windows-x64.zip` — `.dll` built on `windows-2022`
     with **MSVC + Qt6** (same ABI as the official Windows OpenRGB). hidapi is
     compiled **statically inside** the plugin (vendored in `sdk/hidapi/`,
     BSD-3-Clause), so no extra DLL is needed. Untested on real hardware yet:
     consider it *beta*.
2. **Build from source** — follow the instructions below; this is also the
   only path for very old distros (glibc < 2.35, e.g. Ubuntu 20.04 / Debian 11)
   or for Qt5 builds of OpenRGB.

A release is published by pushing a tag: `git tag v1.0.0 && git push --tags`.
The workflow also lets you run the build manually without a release (Actions →
*Release builds* → *Run workflow*) and download the artifacts.

> **`.so` files are Linux-only.** Binary format is platform-specific:
> `.so` (ELF) = Linux, `.dll` (PE) = Windows, `.dylib` (Mach-O) = macOS.
> Always pick the ZIP matching your OS.

### Why the glibc caveat on Linux

A binary compiled on a *rolling* distro (like Arch/CachyOS, glibc 2.44)
requires an equally recent glibc and **will not load** on Debian/Ubuntu with
older glibc. Building on Ubuntu 22.04 (glibc 2.35) in CI gives the widest
compatibility instead.

---

## Project layout

```
Akko-OpenRGB-Plugin/
├── CMakeLists.txt              # CMake build (Qt6 default, Qt5 opt-out, hidapi)
├── README.md
├── LICENSE                     # GPL-2.0-or-later
├── .github/workflows/
│   └── release-builds.yml      # CI: .so (Linux) + .dll (Windows) + Release
├── packaging/                  # Install scripts / instructions redistributed with the ZIPs
│   ├── install-linux.sh
│   ├── INSTALL-LINUX.txt
│   └── INSTALL-WINDOWS.txt
├── sdk/                        # Vendored dependencies (no OpenRGB sources needed)
│   ├── OpenRGBPluginInterface.h
│   ├── RGBControllerInterface.h
│   ├── filesystem.h
│   ├── nlohmann/json.hpp
│   └── hidapi/                 # Vendored hidapi 0.14.0 (BSD-3-Clause), built statically on Windows
│       ├── hidapi.h
│       ├── hid_windows.c
│       └── LICENSE
└── src/
    ├── AkkoKeyboardPlugin.h/.cpp      # Plugin entry point (Q_PLUGIN_METADATA)
    ├── AkkoHidDetector.h/.cpp         # hid_enumerate scan + virtual RGB controller registration
    ├── AkkoDeviceBridge.h/.cpp        # EVision V1 HID protocol (DeviceUpdate* callbacks)
    ├── KeyboardEditorWidget.h/.cpp    # Tab UI: palette, picker, status, timers
    ├── KeyboardCanvas.h/.cpp          # QWidget that draws the keyboard
    ├── KeyboardLayout.h               # Generated key layout (108 keys, real geometry)
    └── metadata.json                  # Embedded plugin metadata (Id, ApiVersion…)
```

---

## License

SPDX-License-Identifier: **GPL-2.0-or-later** (see [LICENSE](LICENSE)).
