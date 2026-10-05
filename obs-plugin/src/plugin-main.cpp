#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QMessageBox>
#include <windows.h>
#include <cstdio>
#include "t0g-stream-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("t0g-stream-control", "en-US")

static T0GStreamDock *g_dock = nullptr;
static obs_hotkey_id g_goLive = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id g_endLive = OBS_INVALID_HOTKEY_ID;

static void debug_log(const char *message)
{
    blog(LOG_INFO, "[T0G Stream Control] %s", message);
    OutputDebugStringA("[T0G Stream Control] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    std::fprintf(stderr, "[T0G Stream Control] %s\n", message);
    std::fflush(stderr);
}

static void go_live_hotkey(void*, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed && g_dock) g_dock->triggerGoLive();
}

static void end_live_hotkey(void*, obs_hotkey_id, obs_hotkey_t*, bool pressed)
{
    if (pressed && g_dock) g_dock->triggerEndLive();
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "T0G Stream Control - Twitch and TikTok control dock for OBS Studio";
}

bool obs_module_load(void)
{
    debug_log("obs_module_load entered");

    try {
        debug_log("Requesting OBS main window");
        auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
        if (!mainWindow) {
            debug_log("ERROR: obs_frontend_get_main_window returned null");
            return false;
        }

        debug_log("Creating T0GStreamDock");
        g_dock = new T0GStreamDock(mainWindow);
        debug_log("T0GStreamDock created");

        debug_log("Adding dock to OBS");
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, g_dock);
        g_dock->show();
        debug_log("Dock added and shown");

        debug_log("Registering frontend hotkeys");
        g_goLive = obs_hotkey_register_frontend("t0g_go_live", "T0G: Go Live Selected Platforms", go_live_hotkey, nullptr);
        g_endLive = obs_hotkey_register_frontend("t0g_end_live", "T0G: End Selected Platforms", end_live_hotkey, nullptr);

        debug_log("Plugin load completed successfully");
        return true;
    } catch (const std::exception &e) {
        blog(LOG_ERROR, "[T0G Stream Control] C++ exception during load: %s", e.what());
        return false;
    } catch (...) {
        debug_log("ERROR: Unknown exception during plugin load");
        return false;
    }
}

void obs_module_unload(void)
{
    debug_log("obs_module_unload entered");
    g_dock = nullptr;
}
