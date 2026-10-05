#include "tiktok-output.hpp"
#include "t0g-log.hpp"

#include <obs.h>

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
    if (videoEncoder) {
        obs_encoder_release(videoEncoder);
        videoEncoder = nullptr;
    }
    if (audioEncoder) {
        obs_encoder_release(audioEncoder);
        audioEncoder = nullptr;
    }
    if (service) {
        obs_service_release(service);
        service = nullptr;
    }
}

bool TikTokOutput::configure(const QString &server, const QString &key, QString *error)
{
    T0GLog::write("Direct RTMP: configure requested; server=" + (server.isEmpty() ? QString("<missing>") : server) + "; key=" + (key.isEmpty() ? QString("<missing>") : QString("<hidden>")));
    reset();

    if (server.trimmed().isEmpty() || key.trimmed().isEmpty()) {
        if (error) *error = "TikTok RTMP server and stream key are required.";
        return false;
    }

    obs_data_t *serviceSettings = obs_data_create();
    obs_data_set_string(serviceSettings, "server", server.trimmed().toUtf8().constData());
    obs_data_set_string(serviceSettings, "key", key.trimmed().toUtf8().constData());
    obs_data_set_bool(serviceSettings, "use_auth", false);
    service = obs_service_create("rtmp_custom", "T0G TikTok Direct Test Service",
                                 serviceSettings, nullptr);
    obs_data_release(serviceSettings);
    if (!service) {
        if (error) *error = "OBS could not create the TikTok RTMP service.";
        return false;
    }

    output = obs_output_create("rtmp_output", "T0G TikTok Direct Test Output", nullptr, nullptr);
    if (!output) {
        if (error) *error = "OBS could not create the TikTok RTMP output.";
        reset();
        return false;
    }
    obs_output_set_service(output, service);

    obs_data_t *videoSettings = obs_data_create();
    obs_data_set_int(videoSettings, "bitrate", 3000);
    obs_data_set_int(videoSettings, "keyint_sec", 2);
    obs_data_set_string(videoSettings, "preset2", "p5");
    obs_data_set_string(videoSettings, "tune", "hq");
    obs_data_set_string(videoSettings, "profile", "high");
    obs_data_set_int(videoSettings, "bf", 2);

    videoEncoder = obs_video_encoder_create("obs_nvenc_h264_tex",
                                            "T0G TikTok Direct H264",
                                            videoSettings, nullptr);
    obs_data_release(videoSettings);
    if (!videoEncoder) {
        // CPU fallback keeps the diagnostic usable on systems without NVENC.
        videoSettings = obs_data_create();
        obs_data_set_int(videoSettings, "bitrate", 3000);
        obs_data_set_int(videoSettings, "keyint_sec", 2);
        obs_data_set_string(videoSettings, "rate_control", "CBR");
        obs_data_set_string(videoSettings, "preset", "veryfast");
        obs_data_set_string(videoSettings, "profile", "high");
        videoEncoder = obs_video_encoder_create("obs_x264",
                                                "T0G TikTok Direct H264",
                                                videoSettings, nullptr);
        obs_data_release(videoSettings);
    }

    obs_data_t *audioSettings = obs_data_create();
    obs_data_set_int(audioSettings, "bitrate", 160);
    audioEncoder = obs_audio_encoder_create("ffmpeg_aac",
                                            "T0G TikTok Direct AAC",
                                            audioSettings, 0, nullptr);
    obs_data_release(audioSettings);

    if (!videoEncoder || !audioEncoder) {
        if (error) *error = "OBS could not create dedicated H.264/AAC encoders for the Direct RTMP Test.";
        reset();
        return false;
    }

    obs_encoder_set_video(videoEncoder, obs_get_video());
    obs_encoder_set_audio(audioEncoder, obs_get_audio());

    obs_output_set_video_encoder(output, videoEncoder);
    obs_output_set_audio_encoder(output, audioEncoder, 0);
    T0GLog::write(QString("Direct RTMP: configured; videoEncoder=%1 audioEncoder=%2").arg(QString::fromUtf8(obs_encoder_get_id(videoEncoder)), QString::fromUtf8(obs_encoder_get_id(audioEncoder))));
    return true;
}

bool TikTokOutput::start(QString *error)
{
    T0GLog::write("Direct RTMP: start requested");
    if (!output) {
        if (error) *error = "TikTok output is not configured.";
        return false;
    }
    if (obs_output_active(output))
        return true;

    if (!obs_output_start(output)) {
        const char *lastError = obs_output_get_last_error(output);
        if (error)
            *error = lastError && *lastError ? QString::fromUtf8(lastError)
                                            : QString("OBS failed to start the TikTok Direct RTMP Test output.");
        T0GLog::write("Direct RTMP: start FAILED; " + (error ? *error : QString("unknown error")), LOG_ERROR);
        return false;
    }
    T0GLog::write("Direct RTMP: start accepted");
    return true;
}

void TikTokOutput::stop()
{
    T0GLog::write("Direct RTMP: stop requested");
    if (output && obs_output_active(output))
        obs_output_stop(output);
}

bool TikTokOutput::active() const
{
    return output && obs_output_active(output);
}
