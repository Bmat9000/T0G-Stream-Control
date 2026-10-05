#pragma once
#include <QDockWidget>
#include "chat-message.hpp"

class QVBoxLayout;
class QLabel;
class QScrollArea;
class QWidget;

class ChatMergerDock final : public QDockWidget {
    Q_OBJECT
public:
    explicit ChatMergerDock(QWidget *parent=nullptr);
public slots:
    void addMessage(ChatMessage message);
    void setTwitchState(bool connected, QString detail);
    void setTikTokState(bool connected, QString detail);
private:
    void refreshHeader();
    QWidget *feed{};
    QVBoxLayout *feedLayout{};
    QLabel *status{};
    bool twitchConnected=false;
    bool tiktokConnected=false;
    QString twitchDetail="Waiting";
    QString tiktokDetail="Waiting";
};
