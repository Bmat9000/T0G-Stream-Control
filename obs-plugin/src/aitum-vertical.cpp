#include "aitum-vertical.hpp"
#include "t0g-log.hpp"

#include <obs.h>
#include <obs-module.h>
#include <callback/proc.h>

namespace {
bool callProc(const char *name, calldata_t *cd)
{
    proc_handler_t *handler = obs_get_proc_handler();
    return handler && proc_handler_call(handler, name, cd);
}

constexpr int kVerticalWidth = 1080;
constexpr int kVerticalHeight = 1920;

obs_output_t *getOutputForCanvas(const QString &name, int width, int height)
{
    T0GLog::write(QString("Aitum: lookup output name='%1' canvas=%2x%3").arg(name).arg(width).arg(height));
    calldata_t cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "width", width);
    calldata_set_int(&cd, "height", height);
    calldata_set_string(&cd, "name", name.toUtf8().constData());

    if (!callProc("aitum_vertical_get_stream_output", &cd)) {
        T0GLog::write("Aitum: get_stream_output procedure call FAILED", LOG_ERROR);
        calldata_free(&cd);
        return nullptr;
    }

    auto *output = static_cast<obs_output_t *>(calldata_ptr(&cd, "output"));
    calldata_free(&cd);
    T0GLog::write(QString("Aitum: lookup result=%1").arg(output ? "FOUND" : "NOT FOUND"), output ? LOG_INFO : LOG_WARNING);
    return output;
}

obs_output_t *getOutput(const QString &name)
{
    // Select the actual portrait canvas first. Aitum matches width/height exactly.
    if (auto *output = getOutputForCanvas(name, kVerticalWidth, kVerticalHeight))
        return output;
    T0GLog::write("Aitum: 1080x1920 lookup failed; trying wildcard canvas as compatibility fallback", LOG_WARNING);
    return getOutputForCanvas(name, 0, 0);
}

bool ensureManagedOutput(const QString &server, const QString &key, QString *error)
{
    T0GLog::write("Aitum: T0G TikTok is missing; asking Vertical Canvas to create/manage it through stream settings");

    calldata_t getCd;
    calldata_init(&getCd);
    calldata_set_int(&getCd, "width", kVerticalWidth);
    calldata_set_int(&getCd, "height", kVerticalHeight);
    if (!callProc("aitum_vertical_get_stream_settings", &getCd)) {
        calldata_free(&getCd);
        T0GLog::write("Aitum: get_stream_settings procedure FAILED", LOG_ERROR);
        if (error) *error = "Aitum Vertical did not expose its stream settings.";
        return false;
    }

    auto *existing = static_cast<obs_data_array_t *>(calldata_ptr(&getCd, "outputs"));
    calldata_free(&getCd);
    if (!existing) {
        T0GLog::write("Aitum: get_stream_settings returned no output array", LOG_ERROR);
        if (error) *error = "Aitum Vertical returned no stream output settings.";
        return false;
    }

    obs_data_array_t *updated = obs_data_array_create();
    bool replaced = false;
    const size_t count = obs_data_array_count(existing);
    T0GLog::write(QString("Aitum: current configured stream outputs=%1").arg(count));

    for (size_t i = 0; i < count; ++i) {
        obs_data_t *item = obs_data_array_item(existing, i);
        if (!item) continue;
        const QString name = QString::fromUtf8(obs_data_get_string(item, "name"));
        T0GLog::write(QString("Aitum: existing output[%1] name='%2' enabled=%3").arg(i).arg(name).arg(obs_data_get_bool(item, "enabled") ? "true" : "false"));
        if (name == QString::fromUtf8(AitumVertical::outputName())) {
            obs_data_set_string(item, "stream_server", server.toUtf8().constData());
            obs_data_set_string(item, "stream_key", key.toUtf8().constData());
            obs_data_set_bool(item, "enabled", true);
            replaced = true;
        }
        obs_data_array_push_back(updated, item);
        obs_data_release(item);
    }

    if (!replaced) {
        obs_data_t *item = obs_data_create();
        obs_data_set_string(item, "name", AitumVertical::outputName());
        obs_data_set_string(item, "stream_server", server.toUtf8().constData());
        obs_data_set_string(item, "stream_key", key.toUtf8().constData());
        obs_data_set_bool(item, "enabled", true);
        obs_data_array_push_back(updated, item);
        obs_data_release(item);
        T0GLog::write("Aitum: appended managed output 'T0G TikTok' (key hidden)");
    }

    obs_data_array_release(existing);

    calldata_t setCd;
    calldata_init(&setCd);
    calldata_set_int(&setCd, "width", kVerticalWidth);
    calldata_set_int(&setCd, "height", kVerticalHeight);
    calldata_set_ptr(&setCd, "outputs", updated);
    const bool ok = callProc("aitum_vertical_set_stream_settings", &setCd);
    calldata_free(&setCd);
    obs_data_array_release(updated);

    if (!ok) {
        T0GLog::write("Aitum: set_stream_settings procedure FAILED", LOG_ERROR);
        if (error) *error = "Aitum Vertical rejected the managed T0G TikTok output.";
        return false;
    }

    T0GLog::write("Aitum: managed output settings applied; verifying T0G TikTok exists");
    obs_output_t *verify = getOutput(QString::fromUtf8(AitumVertical::outputName()));
    if (!verify) {
        T0GLog::write("Aitum: T0G TikTok still missing after set_stream_settings", LOG_ERROR);
        if (error) *error = "Aitum Vertical did not create the T0G TikTok output.";
        return false;
    }
    obs_output_release(verify);
    T0GLog::write("Aitum: T0G TikTok created successfully");
    return true;
}
}

bool AitumVertical::available() const
{
    // Do not call aitum_vertical_get_stream_settings here. Aitum Vertical
    // intentionally disables its own Streaming settings UI when that proc is
    // called because it assumes an external multistream controller owns them.
    const bool loaded = obs_get_module("vertical-canvas") != nullptr;
    T0GLog::write(QString("Aitum: vertical-canvas module loaded=%1").arg(loaded ? "true" : "false"));
    return loaded;
}

bool AitumVertical::configureTikTok(const QString &server, const QString &key, QString *error)
{
    T0GLog::write("Aitum: configuring existing output '" + QString(outputName()) + "' (stream key hidden)");
    // Update only the existing T0G TikTok output. Using Aitum's
    // get/set_stream_settings procedures permanently flips its
    // disable_stream_settings flag for this OBS session and displays the
    // misleading "Aitum Multistream" warning.
    obs_output_t *output = getOutput(outputName());
    if (!output) {
        T0GLog::write("Aitum: T0G TikTok output NOT FOUND; auto-create path starting", LOG_WARNING);
        if (!ensureManagedOutput(server, key, error))
            return false;
        output = getOutput(outputName());
        if (!output) {
            T0GLog::write("Aitum: auto-create reported success but output lookup still failed", LOG_ERROR);
            if (error) *error = "T0G created the Aitum output settings, but Aitum did not expose the new output.";
            return false;
        }
    }

    obs_service_t *service = obs_output_get_service(output);
    if (!service) {
        T0GLog::write("Aitum: output found but has NO streaming service", LOG_ERROR);
        obs_output_release(output);
        if (error)
            *error = "T0G TikTok output does not have a streaming service.";
        return false;
    }

    T0GLog::write(QString("Aitum: output found; active=%1 service_id=%2").arg(obs_output_active(output) ? "true" : "false").arg(QString::fromUtf8(obs_service_get_id(service))));
    obs_data_t *settings = obs_service_get_settings(service);
    if (!settings)
        settings = obs_data_create();

    const QString oldServer = QString::fromUtf8(obs_data_get_string(settings, "server"));
    T0GLog::write("Aitum: updating RTMP server from " + (oldServer.isEmpty() ? QString("<empty>") : oldServer) + " to " + server + "; key hidden");
    obs_data_set_string(settings, "server", server.toUtf8().constData());
    obs_data_set_string(settings, "key", key.toUtf8().constData());
    obs_service_update(service, settings);

    obs_data_release(settings);
    // obs_output_get_service() returns a borrowed pointer. Do not release it.
    obs_output_release(output);
    T0GLog::write("Aitum: service credentials updated successfully");
    return true;
}

bool AitumVertical::callOutputCommand(const char *procedure, QString *error) const
{
    calldata_t cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "width", kVerticalWidth);
    calldata_set_int(&cd, "height", kVerticalHeight);
    calldata_set_string(&cd, "name", outputName());
    T0GLog::write(QString("Aitum: calling %1 for canvas %2x%3 output='%4'").arg(procedure).arg(kVerticalWidth).arg(kVerticalHeight).arg(outputName()));
    const bool ok = callProc(procedure, &cd);
    calldata_free(&cd);
    if (!ok) {
        T0GLog::write(QString("Aitum: procedure %1 FAILED").arg(procedure), LOG_ERROR);
        if (error) *error = "Aitum Vertical output command failed.";
    }
    return ok;
}

bool AitumVertical::startTikTok(QString *error)
{
    T0GLog::write("Aitum: requesting Vertical Canvas to start T0G TikTok");
    const bool called = callOutputCommand("aitum_vertical_start_stream_output", error);
    obs_output_t *output = getOutput(outputName());
    const bool activeNow = output && obs_output_active(output);
    if (output) obs_output_release(output);
    T0GLog::write(QString("Aitum: start command %1; output active immediately=%2").arg(called ? "accepted" : "failed", activeNow ? "true" : "false"), called ? LOG_INFO : LOG_ERROR);
    return called;
}

bool AitumVertical::stopTikTok(QString *error)
{
    T0GLog::write("Aitum: requesting Vertical Canvas to stop T0G TikTok");
    return callOutputCommand("aitum_vertical_stop_stream_output", error);
}

bool AitumVertical::active() const
{
    obs_output_t *output = getOutput(outputName());
    const bool isActive = output && obs_output_active(output);
    if (output)
        obs_output_release(output);
    return isActive;
}
