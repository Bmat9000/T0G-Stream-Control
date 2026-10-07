#pragma once
#include <QDockWidget>
#include <QHash>
#include "chat-message.hpp"

class QVBoxLayout; class QLabel; class QScrollArea; class QWidget; class QTimer;

class ChatMergerDock final : public QDockWidget {
    Q_OBJECT
public:
    explicit ChatMergerDock(QWidget *parent=nullptr);
signals:
    void tikTokTestUsernameChanged(QString username);
public slots:
    void addMessage(ChatMessage message);
    void setTwitchState(bool connected, QString detail);
    void setTikTokState(bool connected, QString detail);
    void setTwitchViewers(int viewers);
private:
    void refreshHeader(); void refreshViewers(); void addEventCard(const ChatMessage &message); void flushLikes();
    void openChatSettings();
    QString settingKey(const ChatMessage &m) const;
    bool eventVisible(const ChatMessage &m) const;
    QString cardColor(const ChatMessage &m) const;
    QWidget *feed{}; QVBoxLayout *feedLayout{}; QScrollArea *scrollArea{}; QLabel *status{};
    QLabel *twitchViewerValue{}; QLabel *tiktokViewerValue{}; QLabel *totalViewerValue{};
    bool twitchConnected=false,tiktokConnected=false; int twitchViewers=-1,tiktokViewers=-1;
    QString twitchDetail="Waiting",tiktokDetail="Waiting"; QHash<QString,ChatMessage> pendingLikes; QTimer *likeFlushTimer{};
};
