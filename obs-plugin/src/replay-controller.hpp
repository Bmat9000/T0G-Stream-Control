#pragma once
#include <QObject>
#include <QPointer>
#include <obs.h>
class QTimer;
class ReplayController final : public QObject {
public:
    explicit ReplayController(QObject *parent);
    ~ReplayController() override;
    static ReplayController *instance();
    void start();
    void stop();
    void save();
    void applyHotkey(int action, const QString &sequence);
    QString status() const;
private:
    static void hotkey(void *, obs_hotkey_id, obs_hotkey_t *, bool);
    void poll();
    QObject *verticalDock() const;
    obs_hotkey_id keys[3]{};
    bool wasLive = false;
    unsigned generation = 0;
    QTimer *timer{};
};
