#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include "t0g-stream-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("t0g-stream-control", "en-US")

static T0GStreamDock *g_dock = nullptr;
static obs_hotkey_id g_goLive = OBS_INVALID_HOTKEY_ID;
static obs_hotkey_id g_endLive = OBS_INVALID_HOTKEY_ID;

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
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow) return false;

    g_dock = new T0GStreamDock(mainWindow);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, g_dock);
    g_dock->show();

    g_goLive = obs_hotkey_register_frontend("t0g_go_live", "T0G: Go Live Selected Platforms", go_live_hotkey, nullptr);
    g_endLive = obs_hotkey_register_frontend("t0g_end_live", "T0G: End Selected Platforms", end_live_hotkey, nullptr);
    blog(LOG_INFO, "[T0G Stream Control] plugin loaded");
    return true;
}

void obs_module_unload(void)
{
    g_dock = nullptr;
}
