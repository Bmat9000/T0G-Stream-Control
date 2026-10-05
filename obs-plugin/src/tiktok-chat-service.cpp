#include "tiktok-chat-service.hpp"
#include "credential-store.hpp"
#include "t0g-log.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>

namespace {
QString avatarFromUser(const QJsonObject &user)
{
    for (const char *key : {"profilePictureUrl", "avatarUrl", "avatar_url"}) {
        const QString value = user.value(QString::fromUtf8(key)).toString();
        if (!value.isEmpty())
            return value;
    }

    for (const char *key : {"avatarThumb", "avatarMedium", "avatarLarger"}) {
        const QJsonObject avatar = user.value(QString::fromUtf8(key)).toObject();
        const QJsonArray urls = avatar.value("urlList").toArray();
        if (!urls.isEmpty()) {
            const QString value = urls.first().toString();
            if (!value.isEmpty())
                return value;
        }
    }
    return {};
}

void walkForChat(TikTokChatService *service, const QJsonValue &value)
{
    if (value.isObject()) {
        service->metaObject(); // keep QObject type complete for MSVC
        const QJsonObject object = value.toObject();
        service->processObject(object);
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isObject() || it.value().isArray())
                walkForChat(service, it.value());
        }
    } else if (value.isArray()) {
        for (const QJsonValue &item : value.toArray())
            walkForChat(service, item);
    }
}
}

TikTokChatService::TikTokChatService(QObject *parent) : QObject(parent)
{
    socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);

    connect(reconnectTimer, &QTimer::timeout, this, [this] {
        if (!stopping && !uniqueId.isEmpty())
            start(uniqueId);
    });

    connect(socket, &QWebSocket::connected, this, [this] {
        liveConnected = true;
        emit connectionChanged(true, "Connected");
        T0GLog::write("Chat Merger: TikTok WebSocket connected for @" + uniqueId);
    });

    connect(socket, &QWebSocket::textMessageReceived, this, &TikTokChatService::handleTextMessage);

    connect(socket, &QWebSocket::disconnected, this, [this] {
        liveConnected = false;
        if (stopping) {
            emit connectionChanged(false, "Stopped");
            return;
        }

        const auto code = socket->closeCode();
        const QString reason = socket->closeReason();
        if (code == 4401 || code == 4403) {
            emit connectionChanged(false, "TikTok chat API key rejected");
            T0GLog::write("Chat Merger: TikTok chat authorization rejected; check API key");
            stopping = true;
            return;
        }

        if (code == 4404) {
            emit connectionChanged(false, "Waiting for TikTok LIVE");
            T0GLog::write("Chat Merger: TikTok account is not live yet; retrying");
            scheduleReconnect(5000);
            return;
        }

        emit connectionChanged(false, "Reconnecting...");
        T0GLog::write(QString("Chat Merger: TikTok chat disconnected code=%1 reason=%2")
                      .arg(static_cast<int>(code)).arg(reason.isEmpty() ? "<none>" : reason));
        scheduleReconnect();
    });

    connect(socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!stopping)
            T0GLog::write("Chat Merger: TikTok WebSocket error: " + socket->errorString());
    });
}

bool TikTokChatService::connected() const
{
    return liveConnected && socket->state() == QAbstractSocket::ConnectedState;
}

QString TikTokChatService::normalizedUsername(QString value)
{
    value = value.trimmed();
    value.remove("https://www.tiktok.com/", Qt::CaseInsensitive);
    if (value.endsWith("/live", Qt::CaseInsensitive))
        value.chop(5);
    while (value.startsWith('@'))
        value.remove(0, 1);
    return value.trimmed();
}

void TikTokChatService::start(const QString &username)
{
    const QString normalized = normalizedUsername(username);
    if (normalized.isEmpty()) {
        stop();
        emit connectionChanged(false, "Waiting for TikTok account");
        return;
    }

    const QString apiKey = CredentialStore::read("TikTokChatApiKey").trimmed();
    if (apiKey.isEmpty()) {
        uniqueId = normalized;
        stopping = true;
        reconnectTimer->stop();
        if (socket->state() != QAbstractSocket::UnconnectedState)
            socket->close();
        emit connectionChanged(false, "Set TikTok Chat API key in Settings");
        T0GLog::write("Chat Merger: TikTok username detected but no chat API key is configured");
        return;
    }

    if (connected() && uniqueId.compare(normalized, Qt::CaseInsensitive) == 0)
        return;

    uniqueId = normalized;
    stopping = false;
    liveConnected = false;
    reconnectTimer->stop();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->abort();

    QUrl url("wss://ws.eulerstream.com");
    QUrlQuery query;
    query.addQueryItem("uniqueId", uniqueId);
    query.addQueryItem("apiKey", apiKey);
    query.addQueryItem("features.schemaVersion", "v2");
    query.addQueryItem("features.bundleEvents", "true");
    query.addQueryItem("features.rawMessages", "false");
    query.addQueryItem("features.normalizeUniqueId", "true");
    url.setQuery(query);

    emit connectionChanged(false, "Connecting...");
    T0GLog::write("Chat Merger: connecting TikTok chat for @" + uniqueId + " (API key hidden)");
    socket->open(url);
}

void TikTokChatService::stop()
{
    stopping = true;
    liveConnected = false;
    reconnectTimer->stop();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->close();
}

void TikTokChatService::scheduleReconnect(int delayMs)
{
    if (!stopping && !uniqueId.isEmpty() && !reconnectTimer->isActive()) {
        reconnectTimer->setInterval(delayMs);
        reconnectTimer->start();
    }
}

QString TikTokChatService::firstString(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QString::fromUtf8(key));
        if (value.isString() && !value.toString().isEmpty())
            return value.toString();
        if (value.isDouble())
            return QString::number(static_cast<qint64>(value.toDouble()));
    }
    return {};
}

QJsonObject TikTokChatService::findUserObject(const QJsonObject &object)
{
    for (const char *key : {"user", "author", "sender"}) {
        const QJsonObject user = object.value(QString::fromUtf8(key)).toObject();
        if (!user.isEmpty())
            return user;
    }
    return {};
}

void TikTokChatService::processObject(const QJsonObject &object)
{
    const QString type = firstString(object, {"type", "event", "eventType", "name"});
    const bool explicitChat = type.contains("WebcastChatMessage", Qt::CaseInsensitive) ||
                              type.compare("chat", Qt::CaseInsensitive) == 0;

    QJsonObject payload = object;
    for (const char *key : {"data", "payload", "message"}) {
        const QJsonObject nested = object.value(QString::fromUtf8(key)).toObject();
        if (!nested.isEmpty() && (explicitChat || nested.contains("comment"))) {
            payload = nested;
            break;
        }
    }

    const QString comment = firstString(payload, {"comment", "text"});
    const QJsonObject user = findUserObject(payload);
    if (comment.isEmpty() || user.isEmpty())
        return;

    if (!explicitChat && !payload.contains("comment"))
        return;

    ChatMessage message;
    message.platform = ChatPlatform::TikTok;
    message.userId = firstString(user, {"id", "userId", "user_id", "secUid"});
    message.username = firstString(user, {"uniqueId", "unique_id", "username"});
    message.displayName = firstString(user, {"nickname", "displayName", "display_name"});
    if (message.displayName.isEmpty())
        message.displayName = message.username;
    message.message = comment;

    const QString avatar = avatarFromUser(user);
    if (!avatar.isEmpty())
        message.avatarUrl = QUrl(avatar);

    if (!message.username.isEmpty())
        emit messageReceived(message);
}

void TikTokChatService::handleTextMessage(const QString &text)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        T0GLog::write("Chat Merger: TikTok chat received non-JSON text frame");
        return;
    }

    if (doc.isObject())
        walkForChat(this, doc.object());
    else if (doc.isArray())
        walkForChat(this, doc.array());
}
