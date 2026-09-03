# Cap-LoRa-1262

LoRa chat application for M5Stack CardputerZero and the SX1262 radio built
into the Cap LoRa-1262 accessory.

## Features

- Send and receive LoRa text messages
- Show radio status, RSSI/SNR, SPI probe results, and initialization diagnostics
- Use an SDL desktop display for UI development without Cap hardware

## Dependencies

Run the bootstrap script once after cloning this repository:

```bash
./bootstrap.sh
```

It fetches `lvgl`, `spdlog`, `smooth_ui_toolkit`, and RadioLib under
`dependencies/`. RadioLib supplies the SX1262 driver. SDL builds require SDL2
development files.

## Build

For SDL desktop testing:

```bash
cmake -S . -B build/sdl -DCAP_LORA_USE_SDL=ON
cmake --build build/sdl -j8
```

For a native CardputerZero build:

```bash
cmake -S . -B build/cp0 -DCAP_LORA_USE_SDL=OFF
cmake --build build/cp0 -j8
```

The desktop and device binaries are written to `dist/sdl/` and `dist/device/`
respectively.

Run the tests with:

```bash
cmake -S . -B build/tests -DCAP_LORA_USE_SDL=ON -DBUILD_TESTING=ON
cmake --build build/tests -j8
ctest --test-dir build/tests --output-on-failure
```

## Usage

Run the SDL build with:

```bash
LV_SDL_ZOOM=2 ./dist/sdl/M5CardputerZero-Cap-LoRa-1262
```

Key controls:

- `Z`/`C` or Left/Right: switch between Messages and Info
- `F`/`X` or Up/Down: scroll messages or switch views
- Enter: compose a message or retry initialization after an error
- Esc: close a dialog or exit

The SDL build displays the complete LoRa UI and uses a simulated backend for
initialization, send, and receive/echo testing. It does not access SPI, GPIO,
or I2C; real hardware initialization is performed only by a device build on a
CardputerZero with the Cap attached.

## Hardware

The device build initializes the Cap's SX1262 over SPI and controls reset,
busy, IRQ, and power through the CardputerZero GPIO/I2C interfaces. The
backend accepts these optional environment-variable overrides (the defaults
match the CardputerZero Cap wiring):

| Variable | Purpose |
| --- | --- |
| `LORA_SPI_DEV` | Explicit SPI device path (otherwise probes `/dev/spidev0.1`, then `/dev/spidev0.0`) |
| `LORA_RST_CHIP`, `LORA_RST_OFFSET` | GPIO chip and line for SX1262 reset |
| `LORA_BUSY_CHIP`, `LORA_BUSY_OFFSET` | GPIO chip and line for SX1262 BUSY |
| `LORA_IRQ_CHIP`, `LORA_IRQ_OFFSET` | GPIO chip and line for SX1262 IRQ |
| `HAT_5VOUT_CHIP`, `HAT_5VOUT_OFFSET` | I2C GPIO chip and line used to enable Cap 5V power |

The Debian package launches the hardware app as root through a non-interactive,
command-specific sudo rule for members of the `gpio` group. The rule permits
only the installed binary with no command arguments. This is currently needed
for `ext_5v_out`; UART access alone normally works for members of `dialout`.

Radio initialization errors and SPI/GPIO diagnostics are shown in the app.

## Package

The executable and package-owned artwork are installed below
`/usr/share/Cap-LoRa-1262/`. The package also installs a desktop entry under
`/usr/share/APPLaunch/applications/`, which is the directory scanned by the
CardputerZero launcher for dynamic applications. Keeping only this desktop
entry in APPLaunch preserves launcher integration without making the LoRa
source or executable part of the APPLaunch project.

Build the CardputerZero `arm64` Debian package on an x86 Linux or WSL2 host with
the Docker wrapper:

```bash
./packaging/docker/package_deb.sh
```

To package natively on a CardputerZero instead, run:

```bash
./packaging/deb/package_deb.sh
```

The generated package is written to `dist/`:

```text
dist/m5cardputerzero-cap-lora-1262_<version>_m5stack1_arm64.deb
```

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for dependency licenses.
