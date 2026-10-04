#include "aitum-vertical.hpp"

#include <obs.h>
#include <obs-module.h>
#include <util/proc.h>

namespace {
bool callProc(const char *name, calldata_t *cd)
{
    proc_handler_t *handler = obs_get_proc_handler();
    return handler && proc_handler_call(handler, name, cd);
}
}

bool AitumVertical::available() const
{
    if (!obs_get_module("vertical-canvas"))
        return false;

    calldata_t cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "width", 0);
    calldata_set_int(&cd, "height", 0);
    const bool ok = callProc("aitum_vertical_get_stream_settings", &cd);
    auto *outputs = static_cast<obs_data_array_t *>(calldata_ptr(&cd, "outputs"));
    if (outputs)
        obs_data_array_release(outputs);
    calldata_free(&cd);
    return ok;
}

bool AitumVertical::configureTikTok(const QString &server, const QString &key, QString *error)
{
    calldata_t get;
    calldata_init(&get);
    calldata_set_int(&get, "width", 0);
    calldata_set_int(&get, "height", 0);

    if (!callProc("aitum_vertical_get_stream_settings", &get)) {
        calldata_free(&get);
        if (error) *error = "Aitum Vertical is not available.";
        return false;
    }

    auto *current = static_cast<obs_data_array_t *>(calldata_ptr(&get, "outputs"));
    obs_data_array_t *updated = obs_data_array_create();
    bool replaced = false;

    if (current) {
        const size_t count = obs_data_array_count(current);
        for (size_t i = 0; i < count; ++i) {
            obs_data_t *item = obs_data_array_item(current, i);
            const QString name = QString::fromUtf8(obs_data_get_string(item, "name"));
            if (name == outputName()) {
                obs_data_set_string(item, "stream_server", server.toUtf8().constData());
                obs_data_set_string(item, "stream_key", key.toUtf8().constData());
                obs_data_set_bool(item, "enabled", true);
                replaced = true;
            }
            obs_data_array_push_back(updated, item);
            obs_data_release(item);
        }
        obs_data_array_release(current);
    }
    calldata_free(&get);

    if (!replaced) {
        obs_data_t *item = obs_data_create();
        obs_data_set_string(item, "name", outputName());
        obs_data_set_string(item, "stream_server", server.toUtf8().constData());
        obs_data_set_string(item, "stream_key", key.toUtf8().constData());
        obs_data_set_bool(item, "enabled", true);
        obs_data_array_push_back(updated, item);
        obs_data_release(item);
    }

    calldata_t set;
    calldata_init(&set);
    calldata_set_int(&set, "width", 0);
    calldata_set_int(&set, "height", 0);
    calldata_set_ptr(&set, "outputs", updated);
    const bool ok = callProc("aitum_vertical_set_stream_settings", &set);
    calldata_free(&set);
    obs_data_array_release(updated);

    if (!ok && error)
        *error = "Aitum Vertical rejected the TikTok output settings.";
    return ok;
}

bool AitumVertical::callOutputCommand(const char *procedure, QString *error) const
{
    calldata_t cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "width", 0);
    calldata_set_int(&cd, "height", 0);
    calldata_set_string(&cd, "name", outputName());
    const bool ok = callProc(procedure, &cd);
    calldata_free(&cd);
    if (!ok && error)
        *error = "Aitum Vertical output command failed.";
    return ok;
}

bool AitumVertical::startTikTok(QString *error)
{
    return callOutputCommand("aitum_vertical_start_stream_output", error);
}

bool AitumVertical::stopTikTok(QString *error)
{
    return callOutputCommand("aitum_vertical_stop_stream_output", error);
}

bool AitumVertical::active() const
{
    calldata_t cd;
    calldata_init(&cd);
    calldata_set_int(&cd, "width", 0);
    calldata_set_int(&cd, "height", 0);
    calldata_set_string(&cd, "name", outputName());

    if (!callProc("aitum_vertical_get_stream_output", &cd)) {
        calldata_free(&cd);
        return false;
    }

    auto *output = static_cast<obs_output_t *>(calldata_ptr(&cd, "output"));
    const bool isActive = output && obs_output_active(output);
    if (output)
        obs_output_release(output);
    calldata_free(&cd);
    return isActive;
}
