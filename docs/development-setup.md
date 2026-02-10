# Development Environment Setup

## Prerequisites

- **VS Code** (1.85+)
- **ESP-IDF v5.x** (installed via the VS Code extension or standalone)
- **USB cable** (USB-C for the ESP32-C3 devkit)

## 1. Install ESP-IDF VS Code Extension

1. Open VS Code.
2. Go to Extensions (Ctrl+Shift+X).
3. Search for **"Espressif IDF"** and install it.
   (The extension ID is `espressif.esp-idf-extension`.)
4. When prompted by the extension, choose **"EXPRESS"** setup:
   - Select ESP-IDF version: **v5.3** (or latest 5.x).
   - Let it download the toolchain and tools automatically.
   - This takes a few minutes on first install.

The C/C++ extension (`ms-vscode.cpptools`) is also recommended and will
be suggested automatically when you open the workspace.

## 2. Open the Project

Open the workspace file from VS Code:

```
File → Open Workspace from File → traincontrol.code-workspace
```

Or from the command line:

```bash
code traincontrol.code-workspace
```

The workspace is configured so that the ESP-IDF extension finds the
project in `sw/` and places build output in `sw/build/`.

## 3. Select Target and Port

Use the ESP-IDF status bar at the bottom of VS Code:

1. **Set target**: Click the chip icon → select **esp32c3**.
2. **Set port**: Click the port selector → choose your USB device
   (typically `/dev/ttyACM0` on Linux).

Alternatively, use the command palette (Ctrl+Shift+P):
- `ESP-IDF: Set Espressif Device Target`
- `ESP-IDF: Select Port to Use`

## 4. Build

Click the **build** button (cylinder icon) in the status bar, or:

```
Ctrl+Shift+P → ESP-IDF: Build your Project
```

From the terminal (if ESP-IDF is sourced):

```bash
cd sw
idf.py set-target esp32c3
idf.py build
```

## 5. Flash

Connect the ESP32-C3 devkit via USB, then click the **flash** button
in the status bar, or:

```
Ctrl+Shift+P → ESP-IDF: Flash your Project
```

Terminal equivalent:

```bash
cd sw
idf.py -p /dev/ttyACM0 flash
```

## 6. Monitor

Click the **monitor** button (screen icon) in the status bar, or:

```
Ctrl+Shift+P → ESP-IDF: Monitor Device
```

Terminal equivalent:

```bash
cd sw
idf.py -p /dev/ttyACM0 monitor
```

Press `Ctrl+]` to exit the monitor.

## 7. Build + Flash + Monitor (Combined)

The most common workflow during development:

```bash
cd sw
idf.py -p /dev/ttyACM0 flash monitor
```

Or use the combined button in the VS Code status bar.

## Project Structure

```
traincontrol/
├── .vscode/
│   ├── settings.json          ← VS Code + ESP-IDF settings
│   └── extensions.json        ← recommended extensions
├── traincontrol.code-workspace
├── docs/
│   ├── high-level-design.md   ← system architecture
│   └── development-setup.md   ← this document
├── hw/                        ← KiCad schematics (future)
└── sw/                        ← ESP-IDF project root
    ├── CMakeLists.txt         ← top-level cmake (includes ESP-IDF)
    ├── sdkconfig.defaults     ← default build config (committed)
    ├── sdkconfig              ← local build config (git-ignored)
    ├── main/
    │   ├── CMakeLists.txt     ← component registration + deps
    │   ├── main.c             ← app_main entry point
    │   ├── motor_control.c/h  ← phase-angle TRIAC control
    │   ├── modbus_server.c/h  ← Modbus TCP server
    │   ├── wifi_manager.c/h   ← WiFi station management
    │   ├── provisioning.c/h   ← SoftAP portal + USB CLI
    │   ├── config.c/h         ← NVS configuration storage
    │   └── status_led.c/h     ← LED pattern driver
    ├── components/            ← local ESP-IDF components (if any)
    └── build/                 ← build output (git-ignored)
```

## Notes

- **sdkconfig vs sdkconfig.defaults**: The `sdkconfig` file is generated
  by the build system and is git-ignored. Persistent configuration changes
  should be made in `sdkconfig.defaults` so they survive a clean build.
  Use `idf.py menuconfig` to explore options, then copy relevant lines
  to `sdkconfig.defaults`.

- **IntelliSense**: After the first successful build, the ESP-IDF extension
  generates `compile_commands.json` in `sw/build/`. The C/C++ extension
  uses this for accurate IntelliSense (includes, defines, completions).
  If IntelliSense shows errors before the first build, this is expected.

- **USB CDC/ACM**: The ESP32-C3 has native USB. The `sdkconfig.defaults`
  enables `CONFIG_ESP_CONSOLE_USB_CDC=y` so that the serial monitor
  works over the native USB port without needing a separate UART adapter.

- **Linux permissions**: If `/dev/ttyACM0` requires root access, add your
  user to the `dialout` group:
  ```bash
  sudo usermod -aG dialout $USER
  ```
  Log out and back in for this to take effect.
