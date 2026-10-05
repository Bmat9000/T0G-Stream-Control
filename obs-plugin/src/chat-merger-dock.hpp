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
    void setTwitchConnected(bool connected);
    void setTikTokConnected(bool connected);
private:
    void refreshHeader();
    QWidget *feed{};
    QVBoxLayout *feedLayout{};
    QLabel *status{};
    bool twitchConnected=false;
    bool tiktokConnected=false;
};
