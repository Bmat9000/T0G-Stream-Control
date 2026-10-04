#pragma once
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

struct T0GSettings {
    bool autoLoadTikTok = true;
    bool rememberStreamInfo = true;
    bool confirmBeforeEnd = true;
    bool startTwitch = true;
    bool startTikTok = true;
    bool stopTwitch = true;
    bool stopTikTok = true;
    bool preferVertical = true;
    QString defaultTitle;
    QString defaultGame;
    int defaultAudience = 0;

    static T0GSettings load();
    void save() const;
};

class T0GSettingsDialog final : public QDialog {
public:
    explicit T0GSettingsDialog(const T0GSettings &settings, QWidget *parent = nullptr);
    T0GSettings settings() const;

private:
    QCheckBox *autoLoadTikTok{};
    QCheckBox *rememberStreamInfo{};
    QCheckBox *confirmBeforeEnd{};
    QCheckBox *startTwitch{};
    QCheckBox *startTikTok{};
    QCheckBox *stopTwitch{};
    QCheckBox *stopTikTok{};
    QCheckBox *preferVertical{};
    QLineEdit *defaultTitle{};
    QLineEdit *defaultGame{};
    QComboBox *defaultAudience{};
};
