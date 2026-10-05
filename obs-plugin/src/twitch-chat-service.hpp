#pragma once
#include <QObject>
#include <QString>
#include "chat-message.hpp"

class QSslSocket;
class QTimer;

class TwitchChatService final : public QObject {
    Q_OBJECT
public:
    explicit TwitchChatService(QObject *parent=nullptr);
    void start(const QString &channelLogin);
    void stop();
    bool connected() const;

signals:
    void messageReceived(ChatMessage message);
    void connectionChanged(bool connected, QString detail);

private:
    void sendLine(const QString &line);
    void handleLine(const QString &line);
    static QString unescapeTag(QString value);

    QSslSocket *socket{};
    QTimer *reconnectTimer{};
    QByteArray buffer;
    QString channel;
    bool joined=false;
    bool stopping=false;
};
