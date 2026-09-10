# AGENTS.md

面向在本仓库中工作的 AI 编程代理的指引。

## 项目概述

Cap-LoRa-1262 是一款面向 M5Stack CardputerZero(搭配 Cap LoRa-1262 配件)的
SX1262 LoRa 文字聊天应用。项目使用 C++17 / CMake,基于 LVGL、spdlog、
smooth_ui_toolkit 以及 RadioLib 的 SX1262 驱动。同一套 UI 代码通过配置期的
`CAP_LORA_USE_SDL` 选项,可在 SDL 桌面后端(开发用,不访问硬件)与
CardputerZero Linux 设备后端(真实 SPI/GPIO/I2C)之间切换。

## 仓库结构

- `src/main.cpp` — 入口:spdlog 初始化、信号处理(APPLaunch 会在 3 秒后
  SIGKILL,因此应用自行设置 2 秒的关机截止时间)、LVGL HAL 初始化、主循环。
- `src/core/` — `LoraApp`:应用生命周期与输入路由。
- `src/views/` — LVGL 界面(`lora_screen.cpp`、`lora_screen_render.cpp`);
  `view.hpp` 定义 `View` 基类。
- `src/models/` — 页面状态模型;`lora_page_contract.hpp` 存放可直接被单元
  测试覆盖的纯 constexpr 策略辅助函数。
- `src/input/` — `GpsKeypad` 键盘输入(仅设备构建)。
- `src/hal/` — LVGL 平台 HAL(SDL 或设备显示/输入)。
- `src/lora/` — 无线电后端。`lora_backend.hpp` 是共享接口;每次只编译其中
  一个实现:
  - `lora_backend_sdl.cpp` — SDL 构建的模拟后端(不访问 SPI/GPIO/I2C);
  - `lora_backend.cpp` + `cp0_lora_*.cpp` — 设备后端:SPI 设备、GPIO
    字符设备、设备发现、帽子电源控制器、PI4IO 扩展器、运行时策略。
- `src/driver/` — 设备驱动骨架(`cap_lora_1262.hpp`,计划中的 Cap LoRa-1262
  驱动组织类)与 PI4IO I2C 扩展器源码。Linux spidev/i2c-dev 用户空间驱动不
  做 vendored 拷贝:设备构建(`CAP_LORA_USE_SDL=OFF`)直接从
  `dependencies/pigweed` 编译显式源码子集(`CAP_LORA_PIGWEED_CPP`,
  pw_spi_linux + pw_i2c_linux 及其依赖,用 C++20 编译),后端选择为
  pw_log_null(日志编译为空操作)、pw_assert_trap、pw_chrono_stl/pw_sync_stl;
  库目标为 `cap_lora_pigweed`。
- `tests/` — 普通可执行文件形式的单元测试与 shell 测试
  (`shutdown_signal_test.sh`、`packaging_scripts_test.sh`);所有测试均在
  `CMakeLists.txt` 中用 `add_test` 注册。
- `packaging/deb/`、`packaging/docker/` — Debian `arm64` 打包(设备上原生
  打包,或在 x86/WSL2 主机上用 Docker 交叉打包)。
- `bootstrap.sh` + `repos.json` + `fetch_repos.py` — 依赖固定与获取,拉取到
  `dependencies/`。
- `lv_conf.h` — LVGL 配置,通过 `LV_BUILD_CONF_PATH` 传给 LVGL。
- `cmake/aarch64-linux-gnu.cmake` — 交叉编译工具链文件。

## 构建与测试

克隆后运行一次(创建 `.venv`,克隆固定版本的依赖):

```bash
./bootstrap.sh
```

SDL 桌面构建(可执行文件输出到 `dist/sdl/`):

```bash
cmake -S . -B build/sdl -DCAP_LORA_USE_SDL=ON
cmake --build build/sdl -j8
LV_SDL_ZOOM=2 ./dist/sdl/M5CardputerZero-Cap-LoRa-1262
```

设备构建(仅限 Linux;可执行文件输出到 `dist/device/`):

```bash
cmake -S . -B build/cp0 -DCAP_LORA_USE_SDL=OFF
cmake --build build/cp0 -j8
```

测试 —— 两个变体都要跑(与 CI 一致):

```bash
cmake -S . -B build/tests -DCAP_LORA_USE_SDL=ON -DBUILD_TESTING=ON
cmake --build build/tests -j8
ctest --test-dir build/tests --output-on-failure

cmake -S . -B build/device-tests -DCAP_LORA_USE_SDL=OFF -DBUILD_TESTING=ON
cmake --build build/device-tests -j8
ctest --test-dir build/device-tests --output-on-failure
```

部分测试只存在于某一个变体:SPI/GPIO/PI4IO/帽子电源/设备发现等资源测试仅
在 `CAP_LORA_USE_SDL=OFF` 时编译;SDL 后端测试与关机信号测试仅在
`CAP_LORA_USE_SDL=ON` 时运行;打包脚本测试两个变体都会运行。

通过 Docker 交叉构建 Debian 包(会下载 CardputerZero BSP 并校验其
SHA-256):

```bash
./packaging/docker/package_deb.sh --clean
```

## 硬性规则

- **绝不修改 `dependencies/` 下的任何内容。** 这些目录是固定到
  `repos.json` 中精确修订版的 git 克隆;若有本地改动或 HEAD 不符,
  `fetch_repos.py` 会直接失败。要更换依赖,请更新 `repos.json` 后重新运行
  `./bootstrap.sh`。
- `CMakeLists.txt` 不做源文件通配收集。新增 `.cpp` 文件必须加入
  `CAP_LORA_APP_CPP`(或对应的测试目标),否则不会被构建。
- RadioLib 只编译显式列出的源码子集(`CAP_LORA_RADIOLIB_CPP`),以避免引入
  Arduino 及无关的无线电协议后端。要用新的 RadioLib 模块,必须把其源文件
  加入该列表。
- 新增第三方依赖需要同步更新 `repos.json`、`THIRD_PARTY_NOTICES.md`,以及
  `packaging/deb/package_deb.sh` 中的许可证安装清单。
- 设备构建(`CAP_LORA_USE_SDL=OFF`)要求 Linux;macOS 仅支持 SDL 构建。
  设备包绝不能链接 SDL2(打包脚本会用 `readelf` 校验)。
- 维持 README "Package" 一节所述的 APPLaunch 集成契约:可执行文件位于
  `/usr/share/Cap-LoRa-1262/`,桌面入口位于
  `/usr/share/APPLaunch/applications/` 且 `Name=LoRa`,图标位于
  `/usr/share/APPLaunch/share/images/`。
- 不得将 udev 规则扩大到 gpio 组对 `ext_5v_out/brightness` 的写权限之外,
  也不得在卸载包时收回 BSP 共享的 `root:gpio` 策略 —— 这两点均由
  `tests/packaging_scripts_test.sh` 守护。
- `build/`、`dist/`、`dependencies/`、`.venv/` 均为生成产物且已 gitignore,
  绝不提交。

## 代码风格

- 使用仓库的 `.clang-format` 格式化(基于 Google 风格,4 空格缩进,120 列
  限制,连续赋值/宏/注释对齐)。
- `.clangd` 从 `build/sdl` 读取编译数据库;请先配置该构建,clangd 才能正常
  工作。
- C++17。GCC/Clang 构建启用 `-Wall -Wextra -Wpedantic`;保持代码零警告。
- 头文件使用 `#pragma once`。
- 命名空间:`cap_lora`(应用核心/模型/类型)、`cap_lora::backend`(无线电
  后端 API)、`cap_gps`(HAL/输入/视图)、小写的 `cp0_lora_*` 用于设备策略
  辅助模块。
- 命名按层区分:UI/HAL/应用层代码使用 `PascalCase`/`camelCase` 函数
  (`initLvglHal`、`onLvglKeyState`)与 `k` 前缀常量
  (`kShutdownTimeoutSeconds`);`src/lora` 下的后端/策略层使用
  `snake_case`(`send_text`、`tx_timeout_for_airtime_us`)与全大写常量
  (`MAX_TEXT_PAYLOAD`、`TX_TIMEOUT_MS`)。与你正在编辑的文件保持一致即可。
  数据成员以下划线结尾(`quit_requested_`)。
- 注释应解释"为什么" —— 尤其是生命周期、安全性与硬件层面的原因 —— 与现有
  代码的做法一致。
- 纯决策逻辑(守卫条件、策略、解析)应放进仅含头文件的 constexpr 函数
  (如 `lora_page_contract.hpp`、`cp0_lora_runtime_policy.hpp`、
  `cp0_lora_gpio_offset_policy.hpp`),以便脱离硬件进行单元测试。

## 测试约定

- 不使用测试框架:每个测试都是普通的 `main()` 可执行文件,使用
  `tests/test_support.hpp` 中的 `CHECK` 宏,并在 `CMakeLists.txt` 中用
  `add_test` 注册。
- shell 测试放在 `tests/*.sh`,同样在 CMake 中注册;它们同时也是打包脚本
  加固的负向测试(非法包名、路径逃逸、过期的 BSP 校验和都必须被拒绝)。
- 后端或策略层的新行为应附带测试;优先测试纯策略函数,而不是去模拟硬件。

## Git 约定

- 提交主题简短、祈使语气("Reject symlink entries in BSP archives");
  也接受 `fix:`/`feat:` 这类约定式前缀。分支模型:`main` 加上
  `fix/...`/功能分支。

## 完成前自查

1. 两个变体的测试套件(SDL 与设备)均以 `ctest --output-on-failure` 通过。
2. 构建在 `-Wall -Wextra -Wpedantic` 下零警告。
3. 新文件已接入 `CMakeLists.txt`(应用源文件、RadioLib 子集或相应测试
   目标)。
4. 依赖/许可证变更已同步更新 `repos.json`、`THIRD_PARTY_NOTICES.md` 与
   打包许可证清单。
5. 若用户可见行为、构建开关或硬件环境变量发生变化,已同步更新 README。
