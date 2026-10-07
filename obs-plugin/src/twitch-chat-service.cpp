#include "twitch-chat-service.hpp"
#include "credential-store.hpp"
#include "t0g-log.hpp"

#include <QSslSocket>
#include <QTimer>
#include <QHash>

TwitchChatService::TwitchChatService(QObject *parent) : QObject(parent)
{
    socket = new QSslSocket(this);
    reconnectTimer = new QTimer(this);
    reconnectTimer->setSingleShot(true);
    reconnectTimer->setInterval(4000);

    connect(reconnectTimer, &QTimer::timeout, this, [this] {
        if (!stopping && !channel.isEmpty())
            start(channel);
    });

    connect(socket, &QSslSocket::encrypted, this, [this] {
        const QString token = CredentialStore::read("TwitchOAuthAccess").trimmed();
        if (token.isEmpty() || channel.isEmpty()) {
            emit connectionChanged(false, "Twitch chat needs a connected Twitch account.");
            stop();
            return;
        }

        // IRC chat is read-only for T0G Chat Merger. Never log the OAuth token.
        sendLine("PASS oauth:" + token);
        sendLine("NICK " + channel);
        sendLine("CAP REQ :twitch.tv/tags twitch.tv/commands");
        sendLine("JOIN #" + channel);
        T0GLog::write("Chat Merger: Twitch IRC TLS connected; joining #" + channel);
    });

    connect(socket, &QSslSocket::readyRead, this, [this] {
        buffer += socket->readAll();
        for (;;) {
            const int end = buffer.indexOf("\r\n");
            if (end < 0)
                break;
            const QString line = QString::fromUtf8(buffer.left(end));
            buffer.remove(0, end + 2);
            handleLine(line);
        }
    });

    connect(socket, &QSslSocket::disconnected, this, [this] {
        const bool wasJoined = joined;
        joined = false;
        emit connectionChanged(false, stopping ? "Stopped" : "Reconnecting...");
        if (wasJoined || !stopping)
            T0GLog::write("Chat Merger: Twitch chat disconnected");
        if (!stopping && !channel.isEmpty() && !reconnectTimer->isActive())
            reconnectTimer->start();
    });

    connect(socket, &QSslSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (!stopping)
            T0GLog::write("Chat Merger: Twitch socket error: " + socket->errorString());
    });
}

bool TwitchChatService::connected() const
{
    return joined && socket->state() == QAbstractSocket::ConnectedState;
}

void TwitchChatService::start(const QString &channelLogin)
{
    const QString normalized = channelLogin.trimmed().toLower();
    if (normalized.isEmpty()) {
        stop();
        emit connectionChanged(false, "Waiting for Twitch login");
        return;
    }

    if (connected() && channel == normalized)
        return;

    stopping = false;
    reconnectTimer->stop();
    if (socket->state() != QAbstractSocket::UnconnectedState)
        socket->abort();

    channel = normalized;
    joined = false;
    buffer.clear();

    T0GLog::write("Chat Merger: connecting Twitch chat for @" + channel);
    emit connectionChanged(false, "Connecting...");
    socket->connectToHostEncrypted("irc.chat.twitch.tv", 6697);
}

void TwitchChatService::stop()
{
    stopping = true;
    reconnectTimer->stop();
    joined = false;
    buffer.clear();
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        sendLine("QUIT");
        socket->disconnectFromHost();
    }
}

void TwitchChatService::sendLine(const QString &line)
{
    if (socket->state() == QAbstractSocket::ConnectedState)
        socket->write(line.toUtf8() + "\r\n");
}

QString TwitchChatService::unescapeTag(QString value)
{
    value.replace("\\s", " ");
    value.replace("\\:", ";");
    value.replace("\\r", "\r");
    value.replace("\\n", "\n");
    value.replace("\\\\", "\\");
    return value;
}

void TwitchChatService::handleLine(const QString &line)
{
    if (line.startsWith("PING ")) {
        sendLine("PONG " + line.mid(5));
        return;
    }

    if (line.contains(" 001 ")) {
        joined = true;
        emit connectionChanged(true, "Connected");
        T0GLog::write("Chat Merger: Twitch chat authenticated");
        return;
    }

    if (line.contains("Login authentication failed", Qt::CaseInsensitive) ||
        line.contains("Improperly formatted auth", Qt::CaseInsensitive)) {
        emit connectionChanged(false, "Reconnect Twitch account for chat permission");
        T0GLog::write("Chat Merger: Twitch chat authentication failed; reconnect Twitch to refresh scopes");
        stopping = true;
        socket->disconnectFromHost();
        return;
    }

    const int privmsg = line.indexOf(" PRIVMSG ");
    if (privmsg < 0)
        return;

    QString tagsPart;
    QString rest = line;
    if (rest.startsWith('@')) {
        const int space = rest.indexOf(' ');
        if (space > 1) {
            tagsPart = rest.mid(1, space - 1);
            rest = rest.mid(space + 1);
        }
    }

    QHash<QString, QString> tags;
    const auto rawTags = tagsPart.split(';', Qt::SkipEmptyParts);
    for (const QString &raw : rawTags) {
        const int eq = raw.indexOf('=');
        if (eq < 0)
            tags.insert(raw, {});
        else
            tags.insert(raw.left(eq), unescapeTag(raw.mid(eq + 1)));
    }

    QString username;
    if (rest.startsWith(':')) {
        const int bang = rest.indexOf('!');
        if (bang > 1)
            username = rest.mid(1, bang - 1);
    }

    const int bodyStart = rest.indexOf(" :");
    if (bodyStart < 0)
        return;

    ChatMessage message;
    message.platform = ChatPlatform::Twitch;
    message.userId = tags.value("user-id");
    message.username = username;
    message.displayName = tags.value("display-name");
    if (message.displayName.isEmpty())
        message.displayName = username;
    message.message = rest.mid(bodyStart + 2);

    if (!message.username.isEmpty() && !message.message.isEmpty())
        emit messageReceived(message);
}
