cmake_minimum_required(VERSION 3.16)

project(M5CardputerZeroCapLoRa1262 VERSION 0.1.1 LANGUAGES C CXX ASM)

option(CAP_LORA_USE_SDL "Use LVGL's SDL backend for desktop development" ON)
option(BUILD_TESTING "Build Cap-LoRa-1262 tests" OFF)
set(CAP_LORA_BIN_NAME "M5CardputerZero-Cap-LoRa-1262" CACHE STRING "Output executable name")
set(CAP_LORA_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/dependencies" CACHE PATH "Third-party source directory")

if(CAP_LORA_USE_SDL)
    set(CAP_LORA_DEFAULT_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/dist/sdl")
else()
    set(CAP_LORA_DEFAULT_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/dist/device")
endif()
if(NOT DEFINED CAP_LORA_OUTPUT_DIR)
    set(CAP_LORA_OUTPUT_DIR "${CAP_LORA_DEFAULT_OUTPUT_DIR}" CACHE PATH "Runtime output directory")
endif()

include(CTest)

if(APPLE AND NOT CAP_LORA_USE_SDL)
    message(FATAL_ERROR "macOS builds only support CAP_LORA_USE_SDL=ON.")
endif()
if(NOT CAP_LORA_USE_SDL AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The CardputerZero hardware backend requires Linux.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CAP_LORA_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(CAP_LORA_SRC_DIR "${CAP_LORA_ROOT}/src")
set(CAP_LORA_LVGL_DIR "${CAP_LORA_DEPS_DIR}/lvgl")
set(CAP_LORA_SPDLOG_DIR "${CAP_LORA_DEPS_DIR}/spdlog")
set(CAP_LORA_SMOOTH_UI_DIR "${CAP_LORA_DEPS_DIR}/smooth_ui_toolkit")
set(CAP_LORA_RADIOLIB_DIR "${CAP_LORA_DEPS_DIR}/RadioLib" CACHE PATH "RadioLib source directory")
set(CAP_LORA_PIGWEED_DIR "${CAP_LORA_DEPS_DIR}/pigweed" CACHE PATH "Pigweed source directory")
set(CAP_LORA_SYSTEM_LIBS ${CMAKE_DL_LIBS})

if(UNIX AND NOT APPLE)
    list(APPEND CAP_LORA_SYSTEM_LIBS m)
endif()
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

foreach(required_dir IN LISTS CAP_LORA_LVGL_DIR CAP_LORA_SPDLOG_DIR CAP_LORA_SMOOTH_UI_DIR)
    if(NOT EXISTS "${required_dir}/CMakeLists.txt")
        message(FATAL_ERROR "Missing dependency: ${required_dir}. Run ./bootstrap.sh first or set CAP_LORA_DEPS_DIR.")
    endif()
endforeach()

find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(LV_BUILD_CONF_PATH "${CAP_LORA_ROOT}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(SMOOTH_UI_TOOLKIT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SMOOTH_UI_TOOLKIT_BUILD_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${CAP_LORA_LVGL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/lvgl" EXCLUDE_FROM_ALL)
add_subdirectory("${CAP_LORA_SPDLOG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/spdlog" EXCLUDE_FROM_ALL)
add_subdirectory("${CAP_LORA_SMOOTH_UI_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/smooth_ui_toolkit"
                 EXCLUDE_FROM_ALL)

function(cap_lora_add_sdl2_parent_include target imported_target)
    if(NOT TARGET "${imported_target}")
        return()
    endif()
    get_target_property(sdl2_include_dirs "${imported_target}" INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT sdl2_include_dirs)
        return()
    endif()
    foreach(sdl2_include_dir IN LISTS sdl2_include_dirs)
        if(sdl2_include_dir MATCHES "/SDL2$")
            get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
            target_include_directories("${target}" SYSTEM PRIVATE "${sdl2_parent_include_dir}")
        endif()
    endforeach()
endfunction()

set(CAP_LORA_LVGL_POSIX_PATH "./")
if(NOT CAP_LORA_USE_SDL)
    set(CAP_LORA_LVGL_POSIX_PATH "/usr/share/Cap-LoRa-1262/")
endif()
target_compile_definitions(lvgl PUBLIC
    CAP_LORA_LVGL_USE_SDL=$<IF:$<BOOL:${CAP_LORA_USE_SDL}>,1,0>
    CAP_LORA_LVGL_FS_POSIX_PATH="${CAP_LORA_LVGL_POSIX_PATH}"
)

if(CAP_LORA_USE_SDL)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()
    if(SDL2_FOUND)
        target_link_libraries(lvgl PRIVATE PkgConfig::SDL2)
        cap_lora_add_sdl2_parent_include(lvgl PkgConfig::SDL2)
        set(CAP_LORA_SDL_LIBS PkgConfig::SDL2)
    else()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            target_link_libraries(lvgl PRIVATE SDL2::SDL2)
            cap_lora_add_sdl2_parent_include(lvgl SDL2::SDL2)
            set(CAP_LORA_SDL_LIBS SDL2::SDL2)
        else()
            find_package(SDL2 REQUIRED)
            target_include_directories(lvgl SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
            foreach(sdl2_include_dir IN LISTS SDL2_INCLUDE_DIRS)
                if(sdl2_include_dir MATCHES "/SDL2$")
                    get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
                    target_include_directories(lvgl SYSTEM PRIVATE "${sdl2_parent_include_dir}")
                endif()
            endforeach()
            set(CAP_LORA_SDL_LIBS ${SDL2_LIBRARIES})
        endif()
    endif()
endif()

# Keep the RadioLib subset used by the Cap's SX1262. Building the selected
# sources avoids pulling in Arduino and unrelated radio protocol backends.
set(CAP_LORA_RADIOLIB_CPP
    "${CAP_LORA_RADIOLIB_DIR}/src/Hal.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/Module.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/utils/CRC.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/utils/ConvCode.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/utils/Utils.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/protocols/PhysicalLayer/PhysicalLayer.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/modules/SX126x/SX126x.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/modules/SX126x/SX126x_commands.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/modules/SX126x/SX126x_config.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/modules/SX126x/SX126x_LR_FHSS.cpp"
    "${CAP_LORA_RADIOLIB_DIR}/src/modules/SX126x/SX1262.cpp"
)
foreach(radiolib_source IN LISTS CAP_LORA_RADIOLIB_CPP)
    if(NOT EXISTS "${radiolib_source}")
        message(FATAL_ERROR "RadioLib source is incomplete: ${radiolib_source}. Run ./bootstrap.sh first.")
    endif()
endforeach()
add_library(cap_lora_radiolib STATIC ${CAP_LORA_RADIOLIB_CPP})
target_include_directories(cap_lora_radiolib SYSTEM PUBLIC "${CAP_LORA_RADIOLIB_DIR}/src")

# Pigweed supplies the Linux userspace bus/GPIO drivers
# (pw_spi_linux, pw_i2c_linux, pw_digital_io_linux) used by the device backend.
# Pigweed's own CMake build wires up more than a
# hundred modules (and pw_i2c_linux has no CMake build at all), so, like the
# RadioLib subset above, only an explicit source subset is compiled here. The
# subset keeps Pigweed's facade/backend structure intact:
#   pw_log                -> pw_log_null (log statements compile out)
#   pw_assert             -> pw_assert_trap (CHECK failures capture and trap)
#   pw_chrono.system_clock -> pw_chrono_stl (std::chrono::steady_clock)
#   pw_sync mutexes       -> pw_sync_stl (std::timed_mutex)
if(NOT CAP_LORA_USE_SDL)
    set(CAP_LORA_PIGWEED_CPP
        "${CAP_LORA_PIGWEED_DIR}/pw_spi_linux/spi.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io/digital_io.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io_linux/digital_io.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io_linux/notifier.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c_linux/initiator.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c/initiator.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c/address.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono/system_clock.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync/mutex.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync/timed_mutex.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync_stl/mutex.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/trap_handler.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_string/format.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_string/string_builder.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_string/type_to_string.cc"
        "${CAP_LORA_PIGWEED_DIR}/pw_status/status.cc"
    )
    set(CAP_LORA_PIGWEED_INCLUDE_DIRS
        # Bus drivers enabled for this project.
        "${CAP_LORA_PIGWEED_DIR}/pw_spi_linux/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_spi/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_bytes/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_span/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io_linux/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c_linux/public"
        # Pigweed interfaces those drivers are built against.
        "${CAP_LORA_PIGWEED_DIR}/pw_spi/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_bytes/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_span/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_status/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_result/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_function/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_thread/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_memory/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_containers/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_numeric/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_string/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_preprocessor/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_polyfill/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_toolchain/public"
        # Facades and the selected backends. The *_overrides directories
        # provide the pw_*_backend/*.h headers the facades include.
        "${CAP_LORA_PIGWEED_DIR}/pw_log/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_log_null/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_log_null/public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/assert_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/check_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono_stl/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono_stl/clock_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync_stl/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync_stl/public_overrides"
        # pw_string/type_to_string.h includes Fuchsia's stdcompat <bit>
        # polyfill that ships inside the Pigweed checkout.
        "${CAP_LORA_PIGWEED_DIR}/third_party/fuchsia/repo/sdk/lib/stdcompat/include"
        "${CAP_LORA_PIGWEED_DIR}/third_party/fuchsia/repo/sdk/lib/fit/include"
    )
    foreach(pigweed_source IN LISTS CAP_LORA_PIGWEED_CPP)
        if(NOT EXISTS "${pigweed_source}")
            message(FATAL_ERROR "Pigweed source is incomplete: ${pigweed_source}. Run ./bootstrap.sh first.")
        endif()
    endforeach()
    foreach(pigweed_include_dir IN LISTS CAP_LORA_PIGWEED_INCLUDE_DIRS)
        if(NOT IS_DIRECTORY "${pigweed_include_dir}")
            message(FATAL_ERROR "Pigweed module directory is missing: ${pigweed_include_dir}. Run ./bootstrap.sh first.")
        endif()
    endforeach()
    add_library(cap_lora_pigweed STATIC ${CAP_LORA_PIGWEED_CPP})
    target_include_directories(cap_lora_pigweed SYSTEM PUBLIC ${CAP_LORA_PIGWEED_INCLUDE_DIRS})
    # pw_digital_io_linux currently leaves a switch helper without a default
    # return; keep the application build warning-clean without modifying the
    # pinned dependency checkout.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cap_lora_pigweed PRIVATE -Wno-return-type)
    endif()
    # Pigweed's sources use C++20 (designated initializers and friends) while
    # the application itself stays on C++17; the library links through the
    # stable C++ ABI either way.
    set_target_properties(cap_lora_pigweed PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
    target_link_libraries(cap_lora_pigweed PUBLIC Threads::Threads)
endif()

set(CAP_LORA_APP_CPP
    "${CAP_LORA_SRC_DIR}/main.cpp"
    "${CAP_LORA_SRC_DIR}/core/lora_app.cpp"
    "${CAP_LORA_SRC_DIR}/input/gps_keypad.cpp"
    "${CAP_LORA_SRC_DIR}/models/lora_page_model.cpp"
    "${CAP_LORA_SRC_DIR}/views/lora_screen.cpp"
    "${CAP_LORA_SRC_DIR}/views/lora_screen_render.cpp"
    "${CAP_LORA_SRC_DIR}/hal/gps_lvgl_hal.cpp"
)
if(CAP_LORA_USE_SDL)
    list(APPEND CAP_LORA_APP_CPP
        "${CAP_LORA_SRC_DIR}/lora/sdl/cap_lora_1262_sdl.cpp"
        "${CAP_LORA_SRC_DIR}/lora/sdl/cap_lora_sdl_assert.cpp")
else()
    list(APPEND CAP_LORA_APP_CPP
        "${CAP_LORA_SRC_DIR}/driver/PI4IO/pi4ioe5v6408_driver.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cap_lora_1262.cpp"
    )
endif()

# Header-only pigweed types (pw::Result, pw::Status) used by the
# CapLoRa1262 facade in both SDL and device builds.  Device builds
# already get these through cap_lora_pigweed; SDL builds need only
# the include paths since Result/Status are header-only for the
# constexpr usage pattern.
add_library(cap_lora_pigweed_headers INTERFACE)
target_include_directories(cap_lora_pigweed_headers SYSTEM INTERFACE
        "${CAP_LORA_RADIOLIB_DIR}/src"
        "${CAP_LORA_PIGWEED_DIR}/pw_spi_linux/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_spi/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_bytes/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_span/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_i2c_linux/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_function/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_thread/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_memory/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_numeric/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_string/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono_stl/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_chrono_stl/clock_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync_stl/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_sync_stl/public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_log_null/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_log_null/public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/assert_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert_trap/check_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_digital_io_linux/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_result/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_status/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_assert/print_and_abort_assert_public_overrides"
        "${CAP_LORA_PIGWEED_DIR}/pw_log/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_containers/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_preprocessor/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_polyfill/public"
        "${CAP_LORA_PIGWEED_DIR}/pw_toolchain/public"
        "${CAP_LORA_PIGWEED_DIR}/third_party/fuchsia/repo/sdk/lib/stdcompat/include"
        "${CAP_LORA_PIGWEED_DIR}/third_party/fuchsia/repo/sdk/lib/fit/include"
)

add_executable(${CAP_LORA_BIN_NAME} ${CAP_LORA_APP_CPP})
target_include_directories(${CAP_LORA_BIN_NAME} PRIVATE "${CAP_LORA_SRC_DIR}")
if(CAP_LORA_USE_SDL)
    target_link_libraries(${CAP_LORA_BIN_NAME} PRIVATE cap_lora_pigweed_headers)
endif()
target_compile_definitions(${CAP_LORA_BIN_NAME} PRIVATE CAP_LORA_USE_SDL=$<BOOL:${CAP_LORA_USE_SDL}>)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${CAP_LORA_BIN_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(${CAP_LORA_BIN_NAME} PRIVATE
    cap_lora_radiolib
    $<$<NOT:$<BOOL:${CAP_LORA_USE_SDL}>>:cap_lora_pigweed>
    lvgl::lvgl
    smooth_ui_toolkit
    spdlog::spdlog
    Threads::Threads
    ${CAP_LORA_SDL_LIBS}
    ${CAP_LORA_SYSTEM_LIBS}
)
set_target_properties(${CAP_LORA_BIN_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CAP_LORA_OUTPUT_DIR}")

if(BUILD_TESTING)
    add_executable(cap_lora_gpio_policy_tests tests/lora_gpio_offset_policy_test.cpp)
    target_include_directories(cap_lora_gpio_policy_tests PRIVATE "${CAP_LORA_SRC_DIR}")
    target_link_libraries(cap_lora_gpio_policy_tests PRIVATE cap_lora_pigweed_headers)
    add_test(NAME cap_lora_gpio_policy COMMAND cap_lora_gpio_policy_tests)

    if(NOT CAP_LORA_USE_SDL)
        add_executable(cap_lora_pigweed_linux_tests tests/pigweed_linux_test.cpp)
        target_include_directories(cap_lora_pigweed_linux_tests PRIVATE
            "${CAP_LORA_ROOT}/tests" "${CAP_LORA_SRC_DIR}")
        target_link_libraries(cap_lora_pigweed_linux_tests PRIVATE cap_lora_pigweed Threads::Threads)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(cap_lora_pigweed_linux_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME cap_lora_pigweed_linux COMMAND cap_lora_pigweed_linux_tests)

        add_executable(cap_lora_pi4io_driver_tests
            tests/pi4io_driver_test.cpp
            src/driver/PI4IO/pi4ioe5v6408_driver.cpp
        )
        target_include_directories(cap_lora_pi4io_driver_tests PRIVATE
            "${CAP_LORA_ROOT}/tests" "${CAP_LORA_SRC_DIR}")
        target_link_libraries(cap_lora_pi4io_driver_tests PRIVATE cap_lora_pigweed Threads::Threads)
        # The fake initiator subclasses pw::i2c::Initiator, whose out-of-line
        # virtuals live in cap_lora_pigweed (built as C++20, where pw::span is
        # an alias to std::span). The test therefore has to use the same
        # dialect, otherwise its pw::span symbols do not match the library's.
        set_target_properties(cap_lora_pi4io_driver_tests PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(cap_lora_pi4io_driver_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME cap_lora_pi4io_driver COMMAND cap_lora_pi4io_driver_tests)

        add_executable(cap_lora_device_discovery_tests
            tests/lora_device_discovery_test.cpp
            src/lora/cap_lora_1262.cpp
            src/driver/PI4IO/pi4ioe5v6408_driver.cpp)
        target_include_directories(cap_lora_device_discovery_tests PRIVATE
            "${CAP_LORA_ROOT}/tests" "${CAP_LORA_SRC_DIR}")
        target_compile_definitions(cap_lora_device_discovery_tests PRIVATE CAP_LORA_USE_SDL=0)
        target_link_libraries(cap_lora_device_discovery_tests PRIVATE cap_lora_radiolib cap_lora_pigweed
            cap_lora_pigweed_headers Threads::Threads)
        add_test(NAME cap_lora_device_discovery COMMAND cap_lora_device_discovery_tests)

    endif()

    add_executable(cap_lora_model_tests tests/lora_page_model_test.cpp src/models/lora_page_model.cpp)
    target_include_directories(cap_lora_model_tests PRIVATE "${CAP_LORA_SRC_DIR}")
    target_link_libraries(cap_lora_model_tests PRIVATE smooth_ui_toolkit spdlog::spdlog lvgl::lvgl
        cap_lora_pigweed_headers Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cap_lora_model_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cap_lora_model COMMAND cap_lora_model_tests)

    add_executable(cap_lora_ui_contract_tests tests/lora_page_ui_contract_test.cpp)
    target_include_directories(cap_lora_ui_contract_tests PRIVATE "${CAP_LORA_ROOT}/tests")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cap_lora_ui_contract_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cap_lora_ui_contract COMMAND cap_lora_ui_contract_tests)

    if(CAP_LORA_USE_SDL)
        add_executable(cap_lora_1262_sdl_tests
            tests/cap_lora_1262_sdl_test.cpp
            src/lora/sdl/cap_lora_1262_sdl.cpp
            src/lora/sdl/cap_lora_sdl_assert.cpp)
        target_include_directories(cap_lora_1262_sdl_tests PRIVATE
            "${CAP_LORA_ROOT}/tests" "${CAP_LORA_SRC_DIR}")
        target_compile_definitions(cap_lora_1262_sdl_tests PRIVATE CAP_LORA_USE_SDL=1)
        target_link_libraries(cap_lora_1262_sdl_tests PRIVATE cap_lora_pigweed_headers Threads::Threads)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(cap_lora_1262_sdl_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME cap_lora_1262_sdl COMMAND cap_lora_1262_sdl_tests)
        add_test(NAME cap_lora_shutdown_signals
            COMMAND bash "${CAP_LORA_ROOT}/tests/shutdown_signal_test.sh"
                    "$<TARGET_FILE:${CAP_LORA_BIN_NAME}>")
        set_tests_properties(cap_lora_shutdown_signals PROPERTIES TIMEOUT 8)
    endif()

    add_test(NAME cap_lora_packaging_scripts
        COMMAND bash "${CAP_LORA_ROOT}/tests/packaging_scripts_test.sh")
endif()
