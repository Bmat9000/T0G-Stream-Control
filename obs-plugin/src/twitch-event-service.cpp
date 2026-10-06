#include "twitch-event-service.hpp"
#include "credential-store.hpp"
#include "t0g-log.hpp"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSslSocket>
#include <QTimer>

namespace {
constexpr const char *kClientId="91gdjfnblk3jb7e2wglcu6hvrv0mi2";

QString tierName(const QString &tier)
{
    if (tier=="3000") return "Tier 3";
    if (tier=="2000") return "Tier 2";
    return "Tier 1";
}

ChatMessage baseTwitchEvent(const QJsonObject &event)
{
    ChatMessage m;
    m.platform=ChatPlatform::Twitch;
    m.userId=event.value("user_id").toString();
    m.username=event.value("user_login").toString();
    m.displayName=event.value("user_name").toString();
    if (m.displayName.isEmpty()) m.displayName=m.username;
    return m;
}
}

TwitchEventService::TwitchEventService(QObject *parent):QObject(parent)
{
    net=new QNetworkAccessManager(this);
    watchdog=new QTimer(this);
    watchdog->setSingleShot(true);
    watchdog->setInterval(45000);
    connect(watchdog,&QTimer::timeout,this,[this]{
        if (!stopping) {
            T0GLog::write("Chat Merger: Twitch EventSub keepalive timed out; reconnecting");
            openSocket();
        }
    });
}

QString TwitchEventService::accessToken() const
{
    return CredentialStore::read("TwitchOAuthAccess").trimmed();
}

void TwitchEventService::start(const QString &id)
{
    if (id.trimmed().isEmpty() || accessToken().isEmpty()) {
        emit stateChanged(false,"Waiting for Twitch account");
        return;
    }
    broadcasterId=id.trimmed();
    stopping=false;
    openSocket();
}

void TwitchEventService::stop()
{
    stopping=true;
    watchdog->stop();
    closeSocket();
}

void TwitchEventService::closeSocket()
{
    if (!socket) return;
    socket->abort();
    socket->deleteLater();
    socket=nullptr;
    handshakeComplete=false;
    socketBuffer.clear();
}

void TwitchEventService::openSocket(const QUrl &url)
{
    closeSocket();
    socketUrl=url;
    handshakeComplete=false;
    socketBuffer.clear();

    QByteArray randomKey(16,Qt::Uninitialized);
    for (int i=0;i<randomKey.size();++i)
        randomKey[i]=static_cast<char>(QRandomGenerator::global()->generate() & 0xff);
    websocketKey=randomKey.toBase64();

    socket=new QSslSocket(this);
    connect(socket,&QSslSocket::encrypted,this,[this]{
        const QByteArray host=socketUrl.host().toUtf8();
        QByteArray path=socketUrl.path(QUrl::FullyEncoded).toUtf8();
        if (path.isEmpty()) path="/";
        if (!socketUrl.query(QUrl::FullyEncoded).isEmpty())
            path+="?"+socketUrl.query(QUrl::FullyEncoded).toUtf8();

        QByteArray request="GET "+path+" HTTP/1.1\r\n";
        request+="Host: "+host+"\r\n";
        request+="Upgrade: websocket\r\nConnection: Upgrade\r\n";
        request+="Sec-WebSocket-Key: "+websocketKey+"\r\n";
        request+="Sec-WebSocket-Version: 13\r\n\r\n";
        socket->write(request);
    });
    connect(socket,&QSslSocket::readyRead,this,&TwitchEventService::handleSocketData);
    connect(socket,&QSslSocket::disconnected,this,[this]{
        if (!stopping) emit stateChanged(false,"Twitch events reconnecting...");
    });
    connect(socket,&QSslSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){
        if (!stopping && socket)
            T0GLog::write("Chat Merger: Twitch EventSub socket error: "+socket->errorString());
    });

    emit stateChanged(false,"Connecting Twitch events...");
    socket->connectToHostEncrypted(socketUrl.host(),socketUrl.port(443));
}

void TwitchEventService::handleSocketData()
{
    if (!socket) return;
    socketBuffer+=socket->readAll();

    if (!handshakeComplete) {
        const int end=socketBuffer.indexOf("\r\n\r\n");
        if (end<0) return;
        const QByteArray headers=socketBuffer.left(end+4);
        socketBuffer.remove(0,end+4);
        if (!headers.startsWith("HTTP/1.1 101")) {
            T0GLog::write("Chat Merger: Twitch EventSub WebSocket handshake failed");
            closeSocket();
            emit stateChanged(false,"Twitch events connection failed");
            return;
        }
        const QByteArray expected=QCryptographicHash::hash(
            websocketKey+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",
            QCryptographicHash::Sha1).toBase64();
        if (!headers.toLower().contains(("sec-websocket-accept: "+expected).toLower())) {
            T0GLog::write("Chat Merger: Twitch EventSub WebSocket handshake validation failed");
            closeSocket();
            emit stateChanged(false,"Twitch events connection failed");
            return;
        }
        handshakeComplete=true;
    }

    for (;;) {
        if (socketBuffer.size()<2) return;
        const quint8 b0=static_cast<quint8>(socketBuffer[0]);
        const quint8 b1=static_cast<quint8>(socketBuffer[1]);
        quint64 length=b1 & 0x7f;
        int header=2;
        if (length==126) {
            if (socketBuffer.size()<4) return;
            length=(static_cast<quint8>(socketBuffer[2])<<8)|static_cast<quint8>(socketBuffer[3]);
            header=4;
        } else if (length==127) {
            if (socketBuffer.size()<10) return;
            length=0;
            for(int i=2;i<10;++i) length=(length<<8)|static_cast<quint8>(socketBuffer[i]);
            header=10;
        }
        if (length>16*1024*1024) {
            T0GLog::write("Chat Merger: Twitch EventSub frame exceeded safety limit");
            openSocket();
            return;
        }
        if (socketBuffer.size()<header+static_cast<qint64>(length)) return;
        const quint8 opcode=b0 & 0x0f;
        const QByteArray payload=socketBuffer.mid(header,static_cast<int>(length));
        socketBuffer.remove(0,header+static_cast<int>(length));

        if (opcode==0x1) {
            handleTextMessage(QString::fromUtf8(payload));
        } else if (opcode==0x8) {
            if (!stopping) openSocket();
            return;
        } else if (opcode==0x9 && socket) {
            // Client-to-server frames must be masked.
            QByteArray mask(4,Qt::Uninitialized);
            for(int i=0;i<4;++i) mask[i]=static_cast<char>(QRandomGenerator::global()->generate()&0xff);
            QByteArray frame;
            frame.append(static_cast<char>(0x8a));
            const int n=payload.size();
            if(n<126) frame.append(static_cast<char>(0x80|n));
            else { frame.append(static_cast<char>(0x80|126)); frame.append(static_cast<char>((n>>8)&0xff)); frame.append(static_cast<char>(n&0xff)); }
            frame+=mask;
            QByteArray masked=payload;
            for(int i=0;i<masked.size();++i) masked[i]=masked[i]^mask[i%4];
            frame+=masked;
            socket->write(frame);
        }
    }
}

void TwitchEventService::handleTextMessage(const QString &text)
{
    watchdog->start();
    const QJsonObject root=QJsonDocument::fromJson(text.toUtf8()).object();
    const QJsonObject metadata=root.value("metadata").toObject();
    const QString messageType=metadata.value("message_type").toString();
    const QJsonObject payload=root.value("payload").toObject();

    if (messageType=="session_welcome") {
        sessionId=payload.value("session").toObject().value("id").toString();
        if (!sessionId.isEmpty()) {
            createSubscriptions(sessionId);
            emit stateChanged(true,"Connected");
            T0GLog::write("Chat Merger: Twitch EventSub connected");
        }
        return;
    }
    if (messageType=="session_keepalive") return;
    if (messageType=="session_reconnect") {
        const QUrl reconnect(payload.value("session").toObject().value("reconnect_url").toString());
        if (reconnect.isValid()) openSocket(reconnect);
        return;
    }
    if (messageType!="notification") return;
    handleNotification(metadata.value("subscription_type").toString(),payload.value("event").toObject());
}

void TwitchEventService::createSubscriptions(const QString &)
{
    QJsonObject broadcaster{{"broadcaster_user_id",broadcasterId}};
    createSubscription("channel.subscribe",broadcaster);
    createSubscription("channel.subscription.gift",broadcaster);
    createSubscription("channel.subscription.message",broadcaster);
    createSubscription("channel.cheer",broadcaster);
    createSubscription("channel.raid",QJsonObject{{"to_broadcaster_user_id",broadcasterId}});
}

void TwitchEventService::createSubscription(const QString &type,const QJsonObject &condition)
{
    QJsonObject body{{"type",type},{"version","1"},{"condition",condition},
                     {"transport",QJsonObject{{"method","websocket"},{"session_id",sessionId}}}};
    QNetworkRequest req(QUrl("https://api.twitch.tv/helix/eventsub/subscriptions"));
    req.setRawHeader("Client-Id",kClientId);
    req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=net->post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[reply,type]{
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString body=QString::fromUtf8(reply->readAll());
        reply->deleteLater();
        if(status!=202) T0GLog::write(QString("Chat Merger: Twitch EventSub %1 subscription failed HTTP %2: %3").arg(type).arg(status).arg(body.left(300)));
    });
}

void TwitchEventService::handleNotification(const QString &type,const QJsonObject &event)
{
    ChatMessage m=baseTwitchEvent(event);
    if(type=="channel.subscribe") {
        if(event.value("is_gift").toBool()) return;
        m.type=ChatEventType::Subscription;
        m.message=QString::fromUtf8("⭐ Subscribed • %1").arg(tierName(event.value("tier").toString()));
    } else if(type=="channel.subscription.message") {
        m.type=ChatEventType::Subscription;
        const int months=event.value("cumulative_months").toInt();
        const QString text=event.value("message").toObject().value("text").toString();
        m.message=QString::fromUtf8("⭐ Resubscribed • %1 month%2").arg(months).arg(months==1?"":"s");
        if(!text.isEmpty()) m.message+=" — "+text;
    } else if(type=="channel.subscription.gift") {
        m.type=ChatEventType::Subscription;
        const bool anon=event.value("is_anonymous").toBool();
        const int total=event.value("total").toInt();
        if(anon){m.username.clear();m.displayName="Anonymous";}
        m.message=QString::fromUtf8("🎁 Gifted %1 sub%2 • %3").arg(total).arg(total==1?"":"s").arg(tierName(event.value("tier").toString()));
    } else if(type=="channel.cheer") {
        m.type=ChatEventType::Gift;
        const bool anon=event.value("is_anonymous").toBool();
        if(anon){m.username.clear();m.displayName="Anonymous";}
        const int bits=event.value("bits").toInt();
        const QString text=event.value("message").toString();
        m.message=QString::fromUtf8("💎 %1 Bit%2").arg(bits).arg(bits==1?"":"s");
        if(!text.isEmpty()) m.message+=" — "+text;
    } else if(type=="channel.raid") {
        m.type=ChatEventType::Share;
        m.userId=event.value("from_broadcaster_user_id").toString();
        m.username=event.value("from_broadcaster_user_login").toString();
        m.displayName=event.value("from_broadcaster_user_name").toString();
        const int viewers=event.value("viewers").toInt();
        m.message=QString::fromUtf8("⚔ Raid with %1 viewer%2").arg(viewers).arg(viewers==1?"":"s");
    } else return;
    emit eventReceived(m);
}
