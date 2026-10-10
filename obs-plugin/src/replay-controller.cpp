#include "replay-controller.hpp"
#include "t0g-settings.hpp"
#include "aitum-vertical.hpp"
#include <obs-frontend-api.h>
#include <callback/proc.h>
#include <util/config-file.h>
#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QKeySequence>
#include <QMessageBox>
#include <QPushButton>
#include <QMetaObject>
#include <cstring>
#include <memory>
namespace {
ReplayController *controller = nullptr;
obs_output_t *verticalOutput()
{
    auto *module = obs_get_module("vertical-canvas");
    if (!module) return nullptr;
    const QString name = QString::fromUtf8(obs_module_get_locale_text(module, "Vertical")) +
        QStringLiteral(" ") + QString::fromUtf8(obs_module_get_locale_text(module, "Backtrack"));
    return obs_get_output_by_name(name.toUtf8().constData());
}
void duration(obs_output_t *output)
{
    if (!output) return;
    auto *data = obs_output_get_settings(output);
    obs_data_set_int(data, "max_time_sec", T0GSettings::load().clipDuration);
    obs_data_set_int(data, "max_size_mb", 0);
    obs_output_update(output, data);
    obs_data_release(data);
}
}
ReplayController *ReplayController::instance() { return controller; }
ReplayController::ReplayController(QObject *parent) : QObject(parent)
{
    controller = this;
    const char *names[] = {"t0g_replay_start", "t0g_replay_stop", "t0g_replay_save"};
    const char *labels[] = {"T0G: Start Replay Buffers", "T0G: Stop Replay Buffers", "T0G: Save Both Clips"};
    QSettings settings("T0G", "T0G Stream Control");
    for (int i = 0; i < 3; ++i) {
        keys[i] = obs_hotkey_register_frontend(names[i], labels[i], hotkey, this);
        const QByteArray json = settings.value(QString("replay/hotkey%1").arg(i)).toByteArray();
        auto *data = obs_data_create_from_json(json.constData());
        if (data) {
            auto *bindings = obs_data_get_array(data, "bindings");
            if (bindings) { obs_hotkey_load(keys[i], bindings); obs_data_array_release(bindings); }
            obs_data_release(data);
        }
    }
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] { poll(); });
    timer->start(500);
}
ReplayController::~ReplayController()
{
    timer->stop();
    QSettings settings("T0G", "T0G Stream Control");
    for (int i = 0; i < 3; ++i) {
        auto *bindings = obs_hotkey_save(keys[i]);
        auto *data = obs_data_create();
        obs_data_set_array(data, "bindings", bindings);
        settings.setValue(QString("replay/hotkey%1").arg(i), QByteArray(obs_data_get_json(data)));
        obs_data_array_release(bindings); obs_data_release(data);
        obs_hotkey_unregister(keys[i]);
    }
    controller = nullptr;
}
QObject *ReplayController::verticalDock() const
{
    auto *window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    for (auto *object : window->findChildren<QObject *>()) {
        const QByteArray name(object->metaObject()->className());
        if ((name == "CanvasDock" || name == "VerticalCanvasDock") &&
            object->metaObject()->indexOfMethod("StartReplayBuffer()") >= 0)
            return object;
    }
    return nullptr;
}
void ReplayController::hotkey(void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
    if (!pressed) return;
    auto *self = static_cast<ReplayController *>(data);
    QMetaObject::invokeMethod(self, [self, id] {
        if (id == self->keys[0]) self->start();
        else if (id == self->keys[1]) self->stop();
        else self->save();
    }, Qt::QueuedConnection);
}
void ReplayController::start()
{
    const unsigned request = ++generation;
    bool newVertical = false;
    auto *config = obs_frontend_get_profile_config();
    if (!config) return;
    const char *mode = config_get_string(config, "Output", "Mode");
    const bool advanced = mode && std::strcmp(mode, "Advanced") == 0;
    config_set_uint(config, advanced ? "AdvOut" : "SimpleOutput", "RecRBTime", T0GSettings::load().clipDuration);
    config_set_uint(config, advanced ? "AdvOut" : "SimpleOutput", "RecRBSize", 0);
    config_save_safe(config, "tmp", nullptr);
    auto *mainOutput = obs_frontend_get_replay_buffer_output();
    if (mainOutput) {
        obs_output_release(mainOutput);
        if (!obs_frontend_replay_buffer_active()) obs_frontend_replay_buffer_start();
    } else {
        QMessageBox::warning(static_cast<QWidget *>(obs_frontend_get_main_window()), "T0G Replay Buffers",
            "Enable Replay Buffer in OBS Settings > Output, apply the change, then use Start Replay Buffers. Vertical can still run if Aitum has its own recording settings.");
    }
    if (auto *dock = verticalDock()) {
        auto *output = verticalOutput();
        const bool active = output && obs_output_active(output);
        if (output) obs_output_release(output);
        if (!active) {
            newVertical = QMetaObject::invokeMethod(dock, "StartReplayBuffer", Qt::DirectConnection);
        }
    }
    QTimer::singleShot(1000, this, [this, request, newVertical] {
        if (request != generation) return;
        // OBS reads retention limits only at output start. Aitum builds its
        // settings internally, so apply T0G's duration before a fresh restart.
        auto *output = newVertical ? verticalOutput() : nullptr;
        if (output) {
            auto *data = obs_output_get_settings(output);
            const bool mismatch = obs_data_get_int(data, "max_time_sec") != T0GSettings::load().clipDuration;
            obs_data_release(data);
            if (mismatch && obs_output_active(output)) {
                obs_output_stop(output);
                auto ref = std::shared_ptr<obs_output_t>(output, obs_output_release);
                auto *wait = new QTimer(this);
                connect(wait, &QTimer::timeout, this, [this, wait, ref, request, attempts = 0]() mutable {
                    if (request != generation) { wait->stop(); wait->deleteLater(); return; }
                    if (!obs_output_active(ref.get())) {
                        wait->stop(); wait->deleteLater();
                        duration(ref.get());
                        if (!obs_output_start(ref.get()))
                            blog(LOG_WARNING, "[T0G Replay] Vertical replay failed to restart with requested duration");
                    } else if (++attempts >= 50) {
                        wait->stop(); wait->deleteLater();
                        blog(LOG_WARNING, "[T0G Replay] Vertical replay duration restart timed out");
                    }
                });
                wait->start(100);
            } else obs_output_release(output);
        }
        blog(LOG_INFO, "[T0G Replay] %s", status().toUtf8().constData());
    });
}
void ReplayController::stop()
{
    ++generation;
    if (obs_frontend_replay_buffer_active()) obs_frontend_replay_buffer_stop();
    if (auto *dock = verticalDock()) QMetaObject::invokeMethod(dock, "StopReplayBuffer", Qt::DirectConnection);
}
void ReplayController::save()
{
    QStringList failures;
    if (obs_frontend_replay_buffer_active()) obs_frontend_replay_buffer_save();
    else failures << "Landscape replay buffer is not running.";
    auto *output = verticalOutput();
    if (output && obs_output_active(output)) {
        calldata_t data; calldata_init(&data);
        if (!proc_handler_call(obs_output_get_proc_handler(output), "save", &data))
            failures << "Vertical clip save request failed.";
        calldata_free(&data);
    } else failures << "Vertical replay buffer is not running or Aitum is unavailable.";
    if (output) obs_output_release(output);
    if (!failures.isEmpty()) QMessageBox::warning(static_cast<QWidget *>(obs_frontend_get_main_window()),
        "T0G Clips", failures.join("\n"));
    // OBS and Aitum report completed saves in their own UI; a request is not a completed file.
}
QString ReplayController::status() const
{
    auto *output = verticalOutput();
    const bool active = output && obs_output_active(output);
    if (output) obs_output_release(output);
    return QString("Landscape: %1 | Vertical: %2").arg(obs_frontend_replay_buffer_active() ? "Running" : "Off",
        active ? "Running" : (verticalDock() ? "Off" : "Unavailable"));
}
void ReplayController::poll()
{
    bool live = obs_frontend_streaming_active();
    // Covers TikTok-only and manual RTMP sessions without treating recording
    // or replay outputs as a live stream.
    obs_enum_outputs([](void *data, obs_output_t *output) {
        if ((obs_output_get_flags(output) & OBS_OUTPUT_SERVICE) && obs_output_active(output) && obs_output_get_total_frames(output) > 0)
            *static_cast<bool *>(data) = true;
        return true;
    }, &live);
    if (T0GSettings::load().autoReplay && live != wasLive) {
        if (live) start(); else stop();
    }
    wasLive = live;
}
void ReplayController::applyHotkey(int action, const QString &sequence)
{
    if (action < 0 || action > 2) return;
    auto *bindings = obs_data_array_create();
    QKeySequence seq(sequence);
    if (!seq.isEmpty()) {
        const auto combination = seq[0];
        const auto mods = combination.keyboardModifiers();
        const int key = combination.key();
        QString obsName;
        if (key >= Qt::Key_F1 && key <= Qt::Key_F35) obsName = QString("OBS_KEY_F%1").arg(key - Qt::Key_F1 + 1);
        else if ((key >= Qt::Key_A && key <= Qt::Key_Z) || (key >= Qt::Key_0 && key <= Qt::Key_9)) obsName = QStringLiteral("OBS_KEY_") + QChar(key);
        else if (key == Qt::Key_Space) obsName = "OBS_KEY_SPACE";
        else if (key == Qt::Key_Insert) obsName = "OBS_KEY_INSERT";
        else if (key == Qt::Key_Delete) obsName = "OBS_KEY_DELETE";
        else if (key == Qt::Key_Home) obsName = "OBS_KEY_HOME";
        else if (key == Qt::Key_End) obsName = "OBS_KEY_END";
        else if (key == Qt::Key_PageUp) obsName = "OBS_KEY_PAGEUP";
        else if (key == Qt::Key_PageDown) obsName = "OBS_KEY_PAGEDOWN";
        if (obsName.isEmpty()) {
            QMessageBox::warning(nullptr, "T0G Hotkey", "Use a letter, number, function key, Space, Insert, Delete, Home, End, Page Up or Page Down. Other keys can be assigned in OBS Settings > Hotkeys.");
            obs_data_array_release(bindings); return;
        }
        auto *binding = obs_data_create();
        obs_data_set_string(binding, "key", obsName.toUtf8().constData());
        obs_data_set_bool(binding, "control", mods.testFlag(Qt::ControlModifier));
        obs_data_set_bool(binding, "shift", mods.testFlag(Qt::ShiftModifier));
        obs_data_set_bool(binding, "alt", mods.testFlag(Qt::AltModifier));
        obs_data_set_bool(binding, "command", mods.testFlag(Qt::MetaModifier));
        obs_data_array_push_back(bindings, binding); obs_data_release(binding);
    }
    obs_hotkey_load(keys[action], bindings);
    auto *data = obs_data_create(); obs_data_set_array(data, "bindings", bindings);
    QSettings settings("T0G", "T0G Stream Control");
    settings.setValue(QString("replay/hotkey%1").arg(action), QByteArray(obs_data_get_json(data)));
    settings.setValue(QString("replay/shortcut%1").arg(action), sequence);
    obs_data_release(data); obs_data_array_release(bindings);
}
