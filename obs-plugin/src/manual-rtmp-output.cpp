#include "manual-rtmp-output.hpp"

#include <obs.h>
#include <obs-frontend-api.h>
#include <utility>

ManualRtmpOutput::ManualRtmpOutput(QString outputName) : name(std::move(outputName)) {}

ManualRtmpOutput::~ManualRtmpOutput()
{
    reset();
}

void ManualRtmpOutput::reset()
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

bool ManualRtmpOutput::configure(const QString &server, const QString &key, QString *error)
{
    reset();
    if (server.trimmed().isEmpty() || key.trimmed().isEmpty()) {
        if (error) *error = "RTMP server and stream key are required.";
        return false;
    }

    obs_data_t *settings = obs_data_create();
    obs_data_set_string(settings, "server", server.trimmed().toUtf8().constData());
    obs_data_set_string(settings, "key", key.trimmed().toUtf8().constData());
    obs_data_set_bool(settings, "use_auth", false);

    const QByteArray serviceName = (name + " Service").toUtf8();
    service = obs_service_create("rtmp_custom", serviceName.constData(), settings, nullptr);
    obs_data_release(settings);
    if (!service) {
        if (error) *error = "OBS could not create the RTMP service.";
        return false;
    }

    const QByteArray outputName = (name + " Output").toUtf8();
    output = obs_output_create("rtmp_output", outputName.constData(), nullptr, nullptr);
    if (!output) {
        if (error) *error = "OBS could not create the RTMP output.";
        reset();
        return false;
    }

    obs_output_set_service(output, service);
    obs_output_t *mainOutput = obs_frontend_get_streaming_output();
    if (!mainOutput) {
        if (error) *error = "Configure OBS streaming encoders first.";
        reset();
        return false;
    }

    obs_encoder_t *video = obs_output_get_video_encoder(mainOutput);
    obs_encoder_t *audio = obs_output_get_audio_encoder(mainOutput, 0);
    if (!video || !audio) {
        if (error) *error = "OBS streaming encoders are not ready.";
        reset();
        return false;
    }

    obs_output_set_video_encoder(output, video);
    obs_output_set_audio_encoder(output, audio, 0);
    return true;
}

bool ManualRtmpOutput::start(QString *error)
{
    if (!output) {
        if (error) *error = "RTMP output is not configured.";
        return false;
    }
    if (obs_output_active(output))
        return true;
    if (!obs_output_start(output)) {
        const char *last = obs_output_get_last_error(output);
        if (error) *error = last && *last ? QString::fromUtf8(last) : QString("OBS failed to start RTMP.");
        return false;
    }
    return true;
}

void ManualRtmpOutput::stop()
{
    if (output && obs_output_active(output))
        obs_output_stop(output);
}

bool ManualRtmpOutput::active() const
{
    return output && obs_output_active(output);
}
