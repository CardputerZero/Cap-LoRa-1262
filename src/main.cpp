#include "core/lora_app.hpp"
#include "hal/gps_lvgl_hal.hpp"
#include "input/gps_keypad.hpp"

#include <core/hal/hal.hpp>
#include <lvgl.h>
#include <spdlog/cfg/env.h>
#include <spdlog/spdlog.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
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
    // Logging libraries are not signal-safe; keep this breadcrumb on write().
    const int saved_errno = errno;
    constexpr char int_message[]  = "Cap-LoRa-1262: received SIGINT; shutdown deadline armed\n";
    constexpr char term_message[] = "Cap-LoRa-1262: received SIGTERM; shutdown deadline armed\n";
    const ssize_t ignored = signal == SIGINT ? ::write(STDERR_FILENO, int_message, sizeof(int_message) - 1)
                                            : ::write(STDERR_FILENO, term_message, sizeof(term_message) - 1);
    (void)ignored;
    errno = saved_errno;
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
    // The deadline uses _exit(), which would otherwise discard buffered logs.
    spdlog::flush_on(spdlog::level::debug);
    installSignalHandlers();

    spdlog::info("Cap-LoRa-1262: lv_init begin");
    lv_init();
    spdlog::info("Cap-LoRa-1262: lv_init complete; HAL initialization begin");
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
        spdlog::info("Cap-LoRa-1262: app construction begin");
        cap_lora::LoraApp app;
        spdlog::info("Cap-LoRa-1262: app construction complete");

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

        spdlog::info("Cap-LoRa-1262: app.start begin");
        app.start();
        spdlog::info("Cap-LoRa-1262: app.start complete");
        lv_obj_invalidate(lv_screen_active());
        spdlog::info("Cap-LoRa-1262: main loop begin");
        while (!app.quitRequested() && !cap_gps::lvglHalQuitRequested() && g_signal_exit_requested == 0) {
#if !LV_USE_SDL
            keypad.poll();
            if (app.quitRequested() || g_signal_exit_requested != 0) {
                break;
            }
#endif
            spdlog::debug("Cap-LoRa-1262: lv_timer_handler begin");
            lv_timer_handler();
            spdlog::debug("Cap-LoRa-1262: lv_timer_handler complete");
            if (cap_gps::lvglHalQuitRequested() || g_signal_exit_requested != 0) {
                break;
            }
            spdlog::debug("Cap-LoRa-1262: app.tick begin");
            app.tick(lv_tick_get());
            spdlog::debug("Cap-LoRa-1262: app.tick complete");
            usleep(10000);
        }

        spdlog::info("Cap-LoRa-1262: exit requested (app={}, display={}, signal={})", app.quitRequested(),
                     cap_gps::lvglHalQuitRequested(), static_cast<int>(g_signal_exit_requested));
        // A signal handler has already started the deadline. Do not move that
        // deadline later; only arm it for exits requested by the UI.
        if (g_signal_exit_requested == 0) alarm(kShutdownTimeoutSeconds);
        spdlog::info("Cap-LoRa-1262: app.stop begin");
        app.stop();
        spdlog::info("Cap-LoRa-1262: app.stop complete");
#if !LV_USE_SDL
        keypad.close();
#endif
        spdlog::info("Cap-LoRa-1262: HAL shutdown begin");
        cap_gps::shutdownLvglHal();
        spdlog::info("Cap-LoRa-1262: HAL shutdown complete; app destruction begin");
        return 0;
    }();
    spdlog::info("Cap-LoRa-1262: app destruction complete (result={})", run_result);
    if (run_result != 0) return run_result;

    // Keep the shutdown deadline active until app/keypad destructors have run.
    alarm(0);
    spdlog::info("Cap-LoRa-1262: shutdown complete");
    return 0;
}
