#pragma once
#include <QDockWidget>
#include <QString>
#include "tiktok-service.hpp"
#include "tiktok-output.hpp"
#include "t0g-settings.hpp"
#include "aitum-vertical.hpp"
#include "manual-rtmp-output.hpp"
#include "twitch-service.hpp"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QListWidget;
class QTimer;

class T0GStreamDock final : public QDockWidget {
public:
    explicit T0GStreamDock(QWidget *parent = nullptr);
    void triggerGoLive() { startSelectedPlatforms(); }
    void triggerEndLive() { stopSelectedPlatforms(); }

private:
    void updateReadyState();
    void loadTikTokFromWeb();
    void refreshTikTokAccount();
    void scheduleGameSearch();
    void runGameSearch();
    void openSettings();
    void openHelpMenu();
    void showPluginLogs();
    void showAitumTikTokSetupGuide();
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
    QLabel *overallStatus{};
    QLabel *verticalStatus{};
    QPushButton *loadTikTokWebButton{};
    QPushButton *refreshTikTokButton{};
    QLabel *tiktokUsername{};
    QLabel *tiktokApproval{};
    QLabel *tiktokCanLive{};
    QListWidget *gameSuggestions{};
    QTimer *gameSearchTimer{};
    QPushButton *settingsButton{};
    QPushButton *helpButton{};
    QPushButton *updateButton{};
    QPushButton *goLiveButton{};
    QPushButton *endLiveButton{};

    TikTokService tiktok;
    TwitchService twitch;
    TikTokOutput tiktokOutput;
    ManualRtmpOutput manualTwitchOutput{"T0G Twitch Manual"};
    ManualRtmpOutput manualTikTokOutput{"T0G TikTok Manual"};
    AitumVertical aitumVertical;
    bool usingAitumVertical = false;
    QString activeTikTokStreamId;
    QString selectedTikTokCategoryId;
    bool busy = false;
    T0GSettings settings;
};
