# WBEE OTA over SIMCOM A7680

## Flash layout

| Region | Address | Size |
|---|---:|---:|
| Factory bootloader | `0x08000000` | 32 KB |
| OTA application | `0x08008000` | 224 KB |
| Download staging | `0x08040000` | 252 KB |
| OTA metadata | `0x0807F000` | 2 KB |
| Device configuration | `0x0807F800` | 2 KB |

The factory must program `Bootloader/build/wbee_bootloader.hex` and the first
OTA application image. The bootloader remains at `0x08000000`; every
application is linked at `0x08008000`.

## Build the bootloader

Run from PowerShell. Pass the GNU Arm toolchain directory when it is not in
`PATH`:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_bootloader.ps1 -ToolchainBin "C:\ST\STM32CubeIDE_1.16.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.win32_1.0.0.202411081344\tools\bin"
```

Program `Bootloader/build/wbee_bootloader.hex` with ST-Link once at the
factory. Do not mass-erase the device during later application programming.

## Build and publish an application

The CubeIDE Debug and Release configurations use
`STM32F103RETX_OTA_APP.ld` and define `OTA_APP_BUILD`. Clean and rebuild the
project, then prepare the GitHub files:

```powershell
python tools/prepare_ota_manifest.py --firmware Debug/wbee_stm32f103ret6.hex --version 2.2.0 --branch feature/ota_mobifone
```

The command creates:

- `ota/wbee_v2.2.0.bin`
- `ota/manifest.json`

Commit and push both files to the branch referenced by `OTA_MANIFEST_URL`.
Only publish a version greater than `VERSION_WBEE` in `Core/Inc/config.h`.

## MQTT trigger

Publish `ota_check` or `ota_update` to:

```text
mobi/water/<device_id>/command/request
```

The application downloads the manifest and binary, validates size and CRC32,
writes a pending metadata record, and resets. The bootloader validates the
staging image, installs it, validates the application copy, clears the pending
record, and jumps to `0x08008000`.

CRC32 detects transfer corruption but is not a cryptographic signature. For a
production threat model, add a signed manifest or signed firmware verification
before marking an image pending.
