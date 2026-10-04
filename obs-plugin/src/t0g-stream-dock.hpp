#pragma once
#include <QDockWidget>
#include <QString>
#include "tiktok-service.hpp"
#include "tiktok-output.hpp"

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
    void loadTikTokToken();
    void startSelectedPlatforms();
    void stopSelectedPlatforms();
    void startTikTok();
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
    QPushButton *updateButton{};
    QPushButton *goLiveButton{};
    QPushButton *endLiveButton{};

    TikTokService tiktok;
    TikTokOutput tiktokOutput;
    QString activeTikTokStreamId;
    bool busy = false;
};
