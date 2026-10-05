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

} // namespace

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
    T0GLog::write("Aitum: refreshing T0G TikTok credentials for this LIVE (stream key hidden)");

    // First require the one-time user-created output. We deliberately do not
    // create outputs through this API because malformed/incomplete entries can
    // destabilize Vertical Canvas.
    obs_output_t *output = getOutput(outputName());
    if (!output) {
        T0GLog::write("Aitum: T0G TikTok output NOT FOUND; open HELP > Aitum TikTok Setup Guide", LOG_ERROR);
        if (error)
            *error = "T0G TikTok output was not found in Aitum Vertical. Open T0G > HELP > Aitum TikTok Setup Guide.";
        return false;
    }
    obs_output_release(output);

    // Aitum keeps its own StreamServer copy of server/key. Updating only the
    // OBS service is not enough: StartStreamOutput() rebuilds the service from
    // that cached copy. Use Aitum's settings API to refresh the EXISTING entry
    // before every start, preserving every other output and all of its settings.
    calldata_t getCd;
    calldata_init(&getCd);
    calldata_set_int(&getCd, "width", kVerticalWidth);
    calldata_set_int(&getCd, "height", kVerticalHeight);
    if (!callProc("aitum_vertical_get_stream_settings", &getCd)) {
        calldata_free(&getCd);
        T0GLog::write("Aitum: could not read stream settings for credential refresh", LOG_ERROR);
        if (error) *error = "T0G could not read Aitum Vertical stream settings.";
        return false;
    }

    auto *outputs = static_cast<obs_data_array_t *>(calldata_ptr(&getCd, "outputs"));
    calldata_free(&getCd);
    if (!outputs) {
        T0GLog::write("Aitum: stream settings returned no outputs", LOG_ERROR);
        if (error) *error = "Aitum Vertical returned no stream outputs.";
        return false;
    }

    bool found = false;
    const size_t count = obs_data_array_count(outputs);
    for (size_t i = 0; i < count; ++i) {
        obs_data_t *item = obs_data_array_item(outputs, i);
        if (!item)
            continue;
        const QString name = QString::fromUtf8(obs_data_get_string(item, "name"));
        if (name == QString::fromUtf8(outputName())) {
            obs_data_set_string(item, "stream_server", server.toUtf8().constData());
            obs_data_set_string(item, "stream_key", key.toUtf8().constData());
            obs_data_set_bool(item, "enabled", true);
            found = true;
            T0GLog::write("Aitum: found T0G TikTok settings; replaced server + current LIVE key (key hidden)");
        }
        obs_data_release(item);
    }

    if (!found) {
        obs_data_array_release(outputs);
        T0GLog::write("Aitum: output existed but settings entry named T0G TikTok was not found", LOG_ERROR);
        if (error) *error = "Aitum's T0G TikTok settings entry could not be found. Open T0G > HELP > Aitum TikTok Setup Guide.";
        return false;
    }

    calldata_t setCd;
    calldata_init(&setCd);
    calldata_set_int(&setCd, "width", kVerticalWidth);
    calldata_set_int(&setCd, "height", kVerticalHeight);
    calldata_set_ptr(&setCd, "outputs", outputs);
    const bool applied = callProc("aitum_vertical_set_stream_settings", &setCd);
    calldata_free(&setCd);
    obs_data_array_release(outputs);

    if (!applied) {
        T0GLog::write("Aitum: failed to apply refreshed LIVE credentials", LOG_ERROR);
        if (error) *error = "Aitum Vertical rejected the refreshed TikTok credentials.";
        return false;
    }

    // Confirm the output survived the update. Never read/log the key here.
    output = getOutput(outputName());
    if (!output) {
        T0GLog::write("Aitum: output missing after credential refresh", LOG_ERROR);
        if (error) *error = "Aitum Vertical lost the T0G TikTok output while refreshing credentials.";
        return false;
    }
    obs_output_release(output);

    T0GLog::write("Aitum: current LIVE server/key synchronized successfully; ready to start");
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
