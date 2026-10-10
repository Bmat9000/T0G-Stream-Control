#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QSettings>
#include <windows.h>
#include <cstdio>
#include "t0g-stream-dock.hpp"
#include "replay-controller.hpp"
#include "chat-merger-dock.hpp"
#include "chat-feed.hpp"
#include "twitch-chat-service.hpp"
#include "twitch-event-service.hpp"
#include "tiktok-chat-service.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("t0g-stream-control", "en-US")

static ReplayController *g_replay = nullptr;
static T0GStreamDock *g_dock = nullptr;
static ChatMergerDock *g_chatDock = nullptr;
static ChatFeed *g_chatFeed = nullptr;
static TwitchChatService *g_twitchChat = nullptr;
static TwitchEventService *g_twitchEvents = nullptr;
static TikTokChatService *g_tiktokChat = nullptr;
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

MODULE_EXPORT bool obs_module_load(void)
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

        debug_log("Creating T0G Chat Merger dock");
        g_chatFeed = new ChatFeed(mainWindow);
        g_chatDock = new ChatMergerDock(mainWindow);
        QObject::connect(g_chatFeed, &ChatFeed::messageReceived, g_chatDock, &ChatMergerDock::addMessage);
        g_twitchChat = new TwitchChatService(mainWindow);
        QObject::connect(g_twitchChat, &TwitchChatService::messageReceived, g_chatFeed, &ChatFeed::publish);
        QObject::connect(g_twitchChat, &TwitchChatService::connectionChanged, g_chatDock,
                         [=](bool connected, const QString &detail) { g_chatDock->setTwitchState(connected, detail); });
        QObject::connect(g_dock, &T0GStreamDock::twitchChatIdentityChanged, g_twitchChat,
                         [=](const QString &login) { g_twitchChat->start(login); });
        g_twitchEvents = new TwitchEventService(mainWindow);
        QObject::connect(g_twitchEvents, &TwitchEventService::eventReceived, g_chatFeed, &ChatFeed::publish);
        QObject::connect(g_twitchEvents, &TwitchEventService::viewerCountChanged, g_chatDock, &ChatMergerDock::setTwitchViewers);
        QObject::connect(g_dock, &T0GStreamDock::twitchEventIdentityChanged, g_twitchEvents,
                         [=](const QString &id) { g_twitchEvents->start(id); });
        g_tiktokChat = new TikTokChatService(mainWindow);
        QObject::connect(g_tiktokChat, &TikTokChatService::messageReceived, g_chatFeed, &ChatFeed::publish);
        QObject::connect(g_tiktokChat, &TikTokChatService::connectionChanged, g_chatDock,
                         [=](bool connected, const QString &detail) { g_chatDock->setTikTokState(connected, detail); });
        QObject::connect(g_dock, &T0GStreamDock::tiktokChatIdentityChanged, g_tiktokChat,
                         [=](const QString &username) {
                             QSettings s("T0G","T0G Chat Merger");
                             const QString test=s.value("tiktok/testUsername").toString().trimmed();
                             g_tiktokChat->start(test.isEmpty()?username:test);
                         });
        QObject::connect(g_chatDock, &ChatMergerDock::tikTokTestUsernameChanged, g_tiktokChat,
                         [=](const QString &username) {
                             const QString target=username.trimmed().isEmpty()?g_dock->tiktokChatUsername():username.trimmed();
                             if(!target.isEmpty()) g_tiktokChat->start(target);
                         });
        const QString existingTwitchLogin = g_dock->twitchChatLogin();
        if (!existingTwitchLogin.isEmpty())
            g_twitchChat->start(existingTwitchLogin);
        const QString existingTwitchId = g_dock->twitchAccountId();
        if (!existingTwitchId.isEmpty())
            g_twitchEvents->start(existingTwitchId);
        QSettings chatSettings("T0G","T0G Chat Merger");
        const QString testTikTok=chatSettings.value("tiktok/testUsername").toString().trimmed();
        if(!testTikTok.isEmpty()) g_tiktokChat->start(testTikTok);
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, g_chatDock);
        g_chatDock->show();
        debug_log("T0G Chat Merger dock added and shown");

        debug_log("Adding dock to OBS");
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, g_dock);
        g_dock->show();
        debug_log("Dock added and shown");

        debug_log("Registering frontend hotkeys");
        g_goLive = obs_hotkey_register_frontend("t0g_go_live", "T0G: Go Live Selected Platforms", go_live_hotkey, nullptr);
        g_endLive = obs_hotkey_register_frontend("t0g_end_live", "T0G: End Selected Platforms", end_live_hotkey, nullptr);

        g_replay = new ReplayController(mainWindow);
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

MODULE_EXPORT void obs_module_unload(void)
{
    debug_log("obs_module_unload entered");
    delete g_replay;
    g_replay = nullptr;
    g_dock = nullptr;
    g_chatDock = nullptr;
    g_chatFeed = nullptr;
    g_twitchChat = nullptr;
    g_twitchEvents = nullptr;
    g_tiktokChat = nullptr;
}
