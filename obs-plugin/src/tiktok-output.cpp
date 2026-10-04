#include "tiktok-output.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

TikTokOutput::~TikTokOutput()
{
    reset();
}

void TikTokOutput::reset()
{
    if (output) {
        if (obs_output_active(output))
            obs_output_stop(output);
        obs_output_release(output);
        output = nullptr;
    }
    if (service) {
        obs_service_release(service);
        service = nullptr;
    }
}

bool TikTokOutput::configure(const QString &server, const QString &key, QString *error)
{
    reset();

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "server", server.toUtf8().constData());
    obs_data_set_string(settings, "key", key.toUtf8().constData());
    obs_data_set_bool(settings, "use_auth", false);

    service = obs_service_create("rtmp_custom", "T0G TikTok Service", settings, nullptr);
    obs_data_release(settings);

    if (!service) {
        if (error) *error = "OBS could not create the TikTok RTMP service.";
        return false;
    }

    output = obs_output_create("rtmp_output", "T0G TikTok Output", nullptr, nullptr);
    if (!output) {
        if (error) *error = "OBS could not create the TikTok RTMP output.";
        reset();
        return false;
    }

    obs_output_set_service(output, service);

    obs_output_t *mainOutput = obs_frontend_get_streaming_output();
    if (!mainOutput) {
        if (error) *error = "Configure your normal OBS streaming output first so T0G can reuse its encoders.";
        reset();
        return false;
    }

    obs_encoder_t *video = obs_output_get_video_encoder(mainOutput);
    obs_encoder_t *audio = obs_output_get_audio_encoder(mainOutput, 0);

    if (!video || !audio) {
        if (error) *error = "OBS streaming encoders are not ready yet.";
        reset();
        return false;
    }

    obs_output_set_video_encoder(output, video);
    obs_output_set_audio_encoder(output, audio, 0);
    return true;
}

bool TikTokOutput::start(QString *error)
{
    if (!output) {
        if (error) *error = "TikTok output is not configured.";
        return false;
    }

    if (obs_output_active(output))
        return true;

    if (!obs_output_start(output)) {
        const char *lastError = obs_output_get_last_error(output);
        if (error) {
            *error = lastError && *lastError ? QString::fromUtf8(lastError)
                                            : QString("OBS failed to start the TikTok output.");
        }
        return false;
    }
    return true;
}

void TikTokOutput::stop()
{
    if (output && obs_output_active(output))
        obs_output_stop(output);
}

bool TikTokOutput::active() const
{
    return output && obs_output_active(output);
}
