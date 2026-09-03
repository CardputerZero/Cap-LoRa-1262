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
    list(APPEND CAP_LORA_APP_CPP "${CAP_LORA_SRC_DIR}/lora/lora_backend_sdl.cpp")
else()
    list(APPEND CAP_LORA_APP_CPP
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_device_discovery.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_gpio.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_hat_power_controller.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_pi4io_controller.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_runtime_policy.cpp"
        "${CAP_LORA_SRC_DIR}/lora/cp0_lora_spi_device.cpp"
    )
    list(APPEND CAP_LORA_APP_CPP "${CAP_LORA_SRC_DIR}/lora/lora_backend.cpp")
endif()

add_executable(${CAP_LORA_BIN_NAME} ${CAP_LORA_APP_CPP})
target_include_directories(${CAP_LORA_BIN_NAME} PRIVATE "${CAP_LORA_SRC_DIR}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${CAP_LORA_BIN_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_link_libraries(${CAP_LORA_BIN_NAME} PRIVATE
    cap_lora_radiolib
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
    add_test(NAME cap_lora_gpio_policy COMMAND cap_lora_gpio_policy_tests)

    add_executable(cap_lora_runtime_policy_tests
        tests/lora_runtime_policy_test.cpp
        src/lora/cp0_lora_runtime_policy.cpp
    )
    target_include_directories(cap_lora_runtime_policy_tests PRIVATE "${CAP_LORA_SRC_DIR}")
    add_test(NAME cap_lora_runtime_policy COMMAND cap_lora_runtime_policy_tests)

    add_executable(cap_lora_model_tests tests/lora_page_model_test.cpp src/models/lora_page_model.cpp)
    target_include_directories(cap_lora_model_tests PRIVATE "${CAP_LORA_SRC_DIR}")
    target_link_libraries(cap_lora_model_tests PRIVATE smooth_ui_toolkit spdlog::spdlog lvgl::lvgl Threads::Threads)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cap_lora_model_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME cap_lora_model COMMAND cap_lora_model_tests)

    if(CAP_LORA_USE_SDL)
        add_executable(cap_lora_sdl_backend_tests
            tests/lora_backend_sdl_test.cpp
            src/lora/lora_backend_sdl.cpp
        )
        target_include_directories(cap_lora_sdl_backend_tests PRIVATE "${CAP_LORA_SRC_DIR}")
        target_link_libraries(cap_lora_sdl_backend_tests PRIVATE Threads::Threads)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(cap_lora_sdl_backend_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME cap_lora_sdl_backend COMMAND cap_lora_sdl_backend_tests)
    endif()
endif()
