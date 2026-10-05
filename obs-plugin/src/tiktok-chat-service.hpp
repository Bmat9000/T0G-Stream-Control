#pragma once
#include <QObject>
#include <QString>
#include "chat-message.hpp"

class QTimer;
class QWebSocket;

class TikTokChatService final : public QObject {
    Q_OBJECT
public:
    explicit TikTokChatService(QObject *parent=nullptr);
    void start(const QString &username);
    void stop();
    bool connected() const;

signals:
    void messageReceived(ChatMessage message);
    void connectionChanged(bool connected, QString detail);

private:
    void scheduleReconnect(int delayMs=5000);
    void handleTextMessage(const QString &text);
    void processObject(const class QJsonObject &object);
    static QString normalizedUsername(QString value);
    static QString firstString(const class QJsonObject &object, std::initializer_list<const char *> keys);
    static class QJsonObject findUserObject(const class QJsonObject &object);

    QWebSocket *socket{};
    QTimer *reconnectTimer{};
    QString uniqueId;
    bool stopping=false;
    bool liveConnected=false;
};
