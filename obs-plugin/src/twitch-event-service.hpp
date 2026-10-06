#pragma once
#include <QObject>
#include <QString>
#include <QUrl>
#include "chat-message.hpp"

class QNetworkAccessManager;
class QSslSocket;
class QTimer;

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
    void handleSocketData();
    void handleTextMessage(const QString &text);
    void createSubscriptions(const QString &sessionId);
    void createSubscription(const QString &type, const QJsonObject &condition);
    void handleNotification(const QString &type, const QJsonObject &event);
    void closeSocket();
    QString accessToken() const;

    QSslSocket *socket{};
    QNetworkAccessManager *net{};
    QTimer *watchdog{};
    QByteArray socketBuffer;
    QByteArray websocketKey;
    QUrl socketUrl;
    QString broadcasterId;
    QString sessionId;
    bool stopping=false;
    bool handshakeComplete=false;
};
