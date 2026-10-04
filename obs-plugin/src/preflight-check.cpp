#include "preflight-check.hpp"
#include "credential-store.hpp"
#include <obs-frontend-api.h>

PreflightResult PreflightCheck::run(const T0GSettings &s, bool twitch, bool tiktok,
                                    const QString &title, bool tiktokTokenLoaded, bool)
{
    PreflightResult r;
    auto fail = [&r](const QString &m) { r.ok = false; r.errors << "• " + m; };
    if (title.trimmed().isEmpty()) fail("Enter a stream title.");
    if (!twitch && !tiktok) fail("Select Twitch, TikTok, or both.");

    if (twitch && s.startTwitch && s.twitchConnectionMode == 1) {
        if (s.twitchManualServer.trimmed().isEmpty()) fail("Twitch manual RTMP server is missing.");
        if (CredentialStore::read("TwitchManualKey").isEmpty()) fail("Twitch manual stream key is missing.");
    }
    if (tiktok && s.startTikTok) {
        if (s.tiktokConnectionMode == 1) {
            if (s.tiktokManualServer.trimmed().isEmpty()) fail("TikTok manual RTMP server is missing.");
            if (CredentialStore::read("TikTokManualKey").isEmpty()) fail("TikTok manual stream key is missing.");
        } else if (!tiktokTokenLoaded) {
            fail("TikTok/Streamlabs is not connected.");
        }
    }

    if ((twitch && s.twitchConnectionMode == 1) || (tiktok && s.tiktokConnectionMode == 1)) {
        obs_output_t *main = obs_frontend_get_streaming_output();
        if (!main) fail("OBS streaming output/encoders are not configured.");
    }
    return r;
}
