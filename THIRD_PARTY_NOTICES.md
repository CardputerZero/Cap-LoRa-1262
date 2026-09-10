# Third-party notices

This project fetches and links the following third-party software:

- LVGL, MIT license
- LodePNG (enabled through LVGL), zlib license
- Montserrat font data (enabled through LVGL), SIL Open Font License 1.1
- spdlog, MIT license
- smooth_ui_toolkit, MIT license
- RadioLib, MIT license
- Pigweed (`pw_spi_linux`, `pw_i2c_linux` and the Pigweed modules they pull
  in: pw_assert, pw_chrono, pw_containers, pw_log, pw_status, pw_string,
  pw_sync and their null/trap/STL backends), Apache-2.0 license
- Fuchsia stdcompat headers (included by Pigweed's `pw_string`), BSD-3-Clause
  license

The corresponding source repositories and pinned revisions are listed in
`repos.json`. The Debian package installs the complete applicable license texts
under `/usr/share/doc/m5cardputerzero-cap-lora-1262/licenses/`.
