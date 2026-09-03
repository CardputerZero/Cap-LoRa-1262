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

The SDL build displays the complete LoRa UI, but hardware initialization remains
unavailable until it is run on a CardputerZero with the Cap attached.

## Hardware

The device build initializes the Cap's SX1262 over SPI and controls reset,
busy, IRQ, and power through the CardputerZero GPIO/I2C interfaces. SPI and
GPIO paths can be overridden with the `CAP_LORA_*` environment variables used
by the backend.

The Debian package launches the hardware app as root through a non-interactive,
command-specific sudo rule for members of the `gpio` group. The rule permits
only the installed binary with no command arguments. This is currently needed
for `ext_5v_out`; UART access alone normally works for members of `dialout`.

Radio initialization errors and SPI/GPIO diagnostics are shown in the app.

## Package

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
