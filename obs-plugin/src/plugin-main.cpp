#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include "t0g-stream-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("t0g-stream-control", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "T0G Stream Control - Twitch and TikTok control dock for OBS Studio";
}

bool obs_module_load(void)
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
        return false;

    auto *dock = new T0GStreamDock(mainWindow);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->show();

    blog(LOG_INFO, "[T0G Stream Control] plugin loaded");
    return true;
}
