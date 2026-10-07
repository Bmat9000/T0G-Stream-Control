#pragma once
#include <QDialog>
#include <QString>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class TwitchService;

struct T0GSettings {
    bool autoLoadTikTok = true;
    bool rememberStreamInfo = true;
    bool confirmBeforeEnd = true;
    bool startTwitch = true;
    bool startTikTok = true;
    bool stopTwitch = true;
    bool stopTikTok = true;
    bool preferVertical = true;
    int tiktokOutputTestMode = 0; // 0 = Aitum Vertical, 1 = Direct OBS RTMP diagnostic
    QString defaultTitle;
    QString defaultGame;
    int defaultAudience = 0;

    int twitchConnectionMode = 0; // 0 = OBS/account, 1 = manual RTMP
    QString twitchManualServer;
    QString twitchClientId;
    int tiktokConnectionMode = 0; // 0 = Streamlabs automatic, 1 = manual RTMP
    QString tiktokManualServer;

    static T0GSettings load();
    void save() const;
};

class T0GSettingsDialog final : public QDialog {
public:
    explicit T0GSettingsDialog(const T0GSettings &settings, QWidget *parent = nullptr);
    T0GSettings settings() const;

private:
    void updateAccountFields();
    void saveSecrets();
    void clearTwitchKey();
    void clearTikTokKey();
    void refreshLiveCredentials();
    void copyField(QLineEdit *field);
    void toggleSecret(QLineEdit *field, QPushButton *button);

    QCheckBox *autoLoadTikTok{};
    QCheckBox *rememberStreamInfo{};
    QCheckBox *confirmBeforeEnd{};
    QCheckBox *startTwitch{};
    QCheckBox *startTikTok{};
    QCheckBox *stopTwitch{};
    QCheckBox *stopTikTok{};
    QCheckBox *preferVertical{};
    QComboBox *tiktokOutputTestMode{};
    QLineEdit *defaultTitle{};
    QLineEdit *defaultGame{};
    QComboBox *defaultAudience{};

    QComboBox *twitchMode{};
    QLineEdit *twitchServer{};
    QLineEdit *twitchKey{};
    QPushButton *clearTwitch{};
    QLabel *twitchAccountStatus{};
    QPushButton *connectTwitch{};
    QPushButton *disconnectTwitch{};
    TwitchService *twitchService{};
    QComboBox *tiktokMode{};
    QLineEdit *tiktokServer{};
    QLineEdit *tiktokKey{};
    QPushButton *clearTikTok{};


    QLineEdit *liveTikTokServer{};
    QLineEdit *liveTikTokKey{};
    QPushButton *showLiveTikTokKey{};
};
