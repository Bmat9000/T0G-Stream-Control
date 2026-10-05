#pragma once
#include <QObject>
#include <QString>
#include "chat-message.hpp"

class TikTokChatService final : public QObject {
    Q_OBJECT
public:
    explicit TikTokChatService(QObject *parent=nullptr);
    ~TikTokChatService() override;
    void start(const QString &username);
    void stop();
    bool connected() const { return liveConnected; }

signals:
    void messageReceived(ChatMessage message);
    void connectionChanged(bool connected, QString detail);

private:
    using Runtime = void;
    using Client = void;
    using Callback = void (*)(int, const char *, size_t, void *);

    static void eventCallback(int type, const char *json, size_t len, void *userData);
    void handleEvent(int type, const QByteArray &json);
    bool loadLocalConnector();
    void unloadLocalConnector();
    static QString normalizedUsername(QString value);

    void *libraryHandle{};
    Runtime *runtime{};
    Client *client{};
    QString uniqueId;
    bool liveConnected=false;

    Runtime *(*fnInit)(){};
    void (*fnShutdown)(Runtime *){};
    Client *(*fnClientNew)(Runtime *, const char *){};
    void (*fnClientFree)(Client *){};
    int (*fnConnect)(Client *, Callback, void *){};
    int (*fnDisconnect)(Client *){};
    const char *(*fnLastError)(){};
};
