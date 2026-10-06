#pragma once
#include <QObject>
#include <QString>
#include "chat-message.hpp"

class QNetworkAccessManager;
class QTimer;
class QWebSocket;

class TwitchEventService final : public QObject {
    Q_OBJECT
public:
    explicit TwitchEventService(QObject *parent=nullptr);
    void start(const QString &broadcasterId);
    void stop();

signals:
    void eventReceived(ChatMessage message);
    void stateChanged(bool connected, QString detail);

private:
    void openSocket(const QUrl &url=QUrl("wss://eventsub.wss.twitch.tv/ws"));
    void handleSocketMessage(const QString &text);
    void createSubscriptions(const QString &sessionId);
    void createSubscription(const QString &type, const QJsonObject &condition);
    void handleNotification(const QString &type, const QJsonObject &event);
    QString accessToken() const;

    QWebSocket *socket{};
    QNetworkAccessManager *net{};
    QTimer *watchdog{};
    QString broadcasterId;
    QString sessionId;
    bool stopping=false;
};
