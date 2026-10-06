#include "twitch-event-service.hpp"
#include "credential-store.hpp"
#include "t0g-log.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

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
    if (socket) {
        socket->close();
        socket->deleteLater();
        socket=nullptr;
    }
}

void TwitchEventService::openSocket(const QUrl &url)
{
    if (socket) {
        socket->abort();
        socket->deleteLater();
    }
    socket=new QWebSocket(QString(),QWebSocketProtocol::VersionLatest,this);
    connect(socket,&QWebSocket::textMessageReceived,this,&TwitchEventService::handleSocketMessage);
    connect(socket,&QWebSocket::disconnected,this,[this]{
        if (!stopping) emit stateChanged(false,"Twitch events reconnecting...");
    });
    connect(socket,&QWebSocket::errorOccurred,this,[this](QAbstractSocket::SocketError){
        if (!stopping) T0GLog::write("Chat Merger: Twitch EventSub socket error: "+socket->errorString());
    });
    emit stateChanged(false,"Connecting Twitch events...");
    socket->open(url);
}

void TwitchEventService::handleSocketMessage(const QString &text)
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

    const QString type=metadata.value("subscription_type").toString();
    handleNotification(type,payload.value("event").toObject());
}

void TwitchEventService::createSubscriptions(const QString &sid)
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
    QJsonObject body;
    body["type"]=type;
    body["version"]="1";
    body["condition"]=condition;
    body["transport"]=QJsonObject{{"method","websocket"},{"session_id",sessionId}};

    QNetworkRequest req(QUrl("https://api.twitch.tv/helix/eventsub/subscriptions"));
    req.setRawHeader("Client-Id",kClientId);
    req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto *reply=net->post(req,QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::finished,this,[reply,type]{
        const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString body=QString::fromUtf8(reply->readAll());
        reply->deleteLater();
        if (status!=202)
            T0GLog::write(QString("Chat Merger: Twitch EventSub %1 subscription failed HTTP %2: %3").arg(type).arg(status).arg(body.left(300)));
    });
}

void TwitchEventService::handleNotification(const QString &type,const QJsonObject &event)
{
    ChatMessage m=baseTwitchEvent(event);
    if (type=="channel.subscribe") {
        if (event.value("is_gift").toBool()) return; // gift event supplies the grouped notification
        m.type=ChatEventType::Subscription;
        m.message=QString::fromUtf8("⭐ Subscribed • %1").arg(tierName(event.value("tier").toString()));
    } else if (type=="channel.subscription.message") {
        m.type=ChatEventType::Subscription;
        const int months=event.value("cumulative_months").toInt();
        const QString text=event.value("message").toObject().value("text").toString();
        m.message=QString::fromUtf8("⭐ Resubscribed • %1 month%2").arg(months).arg(months==1?"":"s");
        if (!text.isEmpty()) m.message+=" — "+text;
    } else if (type=="channel.subscription.gift") {
        m.type=ChatEventType::Subscription;
        const bool anon=event.value("is_anonymous").toBool();
        const int total=event.value("total").toInt();
        if (anon) {
            m.username.clear(); m.displayName="Anonymous";
        }
        m.message=QString::fromUtf8("🎁 Gifted %1 sub%2 • %3").arg(total).arg(total==1?"":"s").arg(tierName(event.value("tier").toString()));
    } else if (type=="channel.cheer") {
        m.type=ChatEventType::Gift;
        const bool anon=event.value("is_anonymous").toBool();
        if (anon) {
            m.username.clear(); m.displayName="Anonymous";
        }
        const int bits=event.value("bits").toInt();
        const QString text=event.value("message").toString();
        m.message=QString::fromUtf8("💎 %1 Bit%2").arg(bits).arg(bits==1?"":"s");
        if (!text.isEmpty()) m.message+=" — "+text;
    } else if (type=="channel.raid") {
        m.type=ChatEventType::Share;
        m.userId=event.value("from_broadcaster_user_id").toString();
        m.username=event.value("from_broadcaster_user_login").toString();
        m.displayName=event.value("from_broadcaster_user_name").toString();
        const int viewers=event.value("viewers").toInt();
        m.message=QString::fromUtf8("⚔ Raid with %1 viewer%2").arg(viewers).arg(viewers==1?"":"s");
    } else {
        return;
    }
    emit eventReceived(m);
}
