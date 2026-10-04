#pragma once
#include <QDockWidget>
#include <QString>
#include "tiktok-service.hpp"
#include "tiktok-output.hpp"
#include "t0g-settings.hpp"
#include "aitum-vertical.hpp"
#include "manual-rtmp-output.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class T0GStreamDock final : public QDockWidget {
public:
    explicit T0GStreamDock(QWidget *parent = nullptr);

private:
    void updateReadyState();
    void loadTikTokToken(bool quiet = false);
    void openSettings();
    void showTikTokFallbackFailure(const QString &detail);
    void loadSavedStreamInfo();
    void saveStreamInfo();
    void startSelectedPlatforms();
    void stopSelectedPlatforms();
    void startTikTok();
    bool startManualTwitch();
    bool startManualTikTok();
    void setBusy(bool busy);
    void setTikTokStatus(const QString &text);
    void setTwitchStatus(const QString &text);

    QLineEdit *titleEdit{};
    QLineEdit *gameEdit{};
    QComboBox *audienceBox{};
    QCheckBox *twitchEnabled{};
    QCheckBox *tiktokEnabled{};
    QLabel *twitchStatus{};
    QLabel *tiktokStatus{};
    QPushButton *loadTikTokButton{};
    QPushButton *settingsButton{};
    QPushButton *updateButton{};
    QPushButton *goLiveButton{};
    QPushButton *endLiveButton{};

    TikTokService tiktok;
    TikTokOutput tiktokOutput;
    ManualRtmpOutput manualTwitchOutput{"T0G Twitch Manual"};
    ManualRtmpOutput manualTikTokOutput{"T0G TikTok Manual"};
    AitumVertical aitumVertical;
    bool usingAitumVertical = false;
    QString activeTikTokStreamId;
    bool busy = false;
    T0GSettings settings;
};
