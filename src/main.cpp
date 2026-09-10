#include "core/lora_app.hpp"
#include "hal/gps_lvgl_hal.hpp"
#include "input/gps_keypad.hpp"

#include <core/hal/hal.hpp>
#include <lvgl.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <csignal>
#include <unistd.h>

namespace {

// APPLaunch sends SIGKILL after a three-second grace period. Keep our own
// deadline shorter so cleanup cannot consume the launcher's entire window.
constexpr unsigned int kShutdownTimeoutSeconds = 2;
volatile std::sig_atomic_t g_signal_exit_requested = 0;

void requestExitFromSignal(int signal)
{
    if (g_signal_exit_requested != 0) return;
    g_signal_exit_requested = signal;
    alarm(kShutdownTimeoutSeconds);
}

void forceExitAfterShutdownTimeout(int)
{
    constexpr char message[] = "Cap-LoRa-1262: shutdown timed out; forcing process exit\n";
    const ssize_t ignored    = ::write(STDERR_FILENO, message, sizeof(message) - 1);
    (void)ignored;
    _exit(2);
}

void installSignalHandlers()
{
    struct sigaction action {};
    action.sa_handler = requestExitFromSignal;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGTERM);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);

    action.sa_handler = forceExitAfterShutdownTimeout;
    sigaction(SIGALRM, &action, nullptr);
}

}  // namespace

int main()
{
    constexpr int32_t kScreenWidth  = 320;
    constexpr int32_t kScreenHeight = 170;

    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [thread %t] %v");
    spdlog::cfg::load_env_levels();
    installSignalHandlers();

    lv_init();
    if (!cap_gps::initLvglHal(kScreenWidth, kScreenHeight)) {
        return 1;
    }
    lv_display_t* display = lv_display_get_default();
    if (!display) {
        std::fprintf(stderr, "Cap-LoRa-1262: failed to create LVGL display\n");
        cap_gps::shutdownLvglHal();
        return 1;
    }

    spdlog::info("Cap-LoRa-1262: display {}x{}", static_cast<int>(lv_display_get_horizontal_resolution(display)),
                 static_cast<int>(lv_display_get_vertical_resolution(display)));
    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return lv_tick_get(); });
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t milliseconds) { usleep(milliseconds * 1000); });

    const int run_result = [&]() -> int {
        cap_lora::LoraApp app;

#if !LV_USE_SDL
        cap_gps::GpsKeypad keypad;
        keypad.setKeyCallback(
            [&app](uint32_t key, const char* utf8, bool pressed) { return app.onLvglKeyState(key, utf8, pressed); });
        if (!keypad.openDefault()) {
            spdlog::error("Cap-LoRa-1262: no usable keyboard input device; aborting startup");
            keypad.close();
            cap_gps::shutdownLvglHal();
            return 1;
        }
#endif

        app.start();
        lv_obj_invalidate(lv_screen_active());
        while (!app.quitRequested() && !cap_gps::lvglHalQuitRequested() && g_signal_exit_requested == 0) {
#if !LV_USE_SDL
            keypad.poll();
            if (app.quitRequested() || g_signal_exit_requested != 0) {
                break;
            }
#endif
            lv_timer_handler();
            if (cap_gps::lvglHalQuitRequested() || g_signal_exit_requested != 0) {
                break;
            }
            app.tick(lv_tick_get());
            usleep(10000);
        }

        spdlog::info("Cap-LoRa-1262: exit requested (app={}, display={}, signal={})", app.quitRequested(),
                     cap_gps::lvglHalQuitRequested(), static_cast<int>(g_signal_exit_requested));
        // A signal handler has already started the deadline. Do not move that
        // deadline later; only arm it for exits requested by the UI.
        if (g_signal_exit_requested == 0) alarm(kShutdownTimeoutSeconds);
        app.stop();
#if !LV_USE_SDL
        keypad.close();
#endif
        cap_gps::shutdownLvglHal();
        return 0;
    }();
    if (run_result != 0) return run_result;

    // Keep the shutdown deadline active until app/keypad destructors have run.
    alarm(0);
    spdlog::info("Cap-LoRa-1262: shutdown complete");
    return 0;
}
