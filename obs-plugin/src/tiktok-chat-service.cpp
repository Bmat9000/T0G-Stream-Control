#include "tiktok-chat-service.hpp"
#include "t0g-log.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
constexpr int kConnected = 0;
constexpr int kReconnecting = 1;
constexpr int kDisconnected = 2;
constexpr int kChat = 10;
constexpr int kGift = 11;
constexpr int kLike = 12;
constexpr int kRoomUserSeq = 15;
constexpr int kFollow = 20;
constexpr int kShare = 21;
constexpr int kJoin = 22;
constexpr int kLiveEnded = 23;
constexpr int kSubNotify = 62;
constexpr int kSubscriptionNotify = 103;

QString firstString(const QJsonObject &o, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QJsonValue v=o.value(QString::fromUtf8(key));
        if (v.isString() && !v.toString().isEmpty()) return v.toString();
        if (v.isDouble()) return QString::number(static_cast<qint64>(v.toDouble()));
    }
    return {};
}
}

TikTokChatService::TikTokChatService(QObject *parent) : QObject(parent) {}

TikTokChatService::~TikTokChatService()
{
    stop();
    unloadLocalConnector();
}

QString TikTokChatService::normalizedUsername(QString value)
{
    value=value.trimmed();
    value.remove("https://www.tiktok.com/", Qt::CaseInsensitive);
    if (value.endsWith("/live", Qt::CaseInsensitive)) value.chop(5);
    while (value.startsWith('@')) value.remove(0,1);
    return value.trimmed();
}

bool TikTokChatService::loadLocalConnector()
{
    if (libraryHandle) return true;
#ifdef Q_OS_WIN
    const QString dllPath=QCoreApplication::applicationDirPath()+"/piratetok.dll";
    HMODULE lib=LoadLibraryW(reinterpret_cast<LPCWSTR>(dllPath.utf16()));
    if (!lib) {
        T0GLog::write("Chat Merger: local TikTok connector missing at "+dllPath);
        return false;
    }
    libraryHandle=lib;
    auto sym=[lib](const char *name)->FARPROC { return GetProcAddress(lib,name); };
    fnInit=reinterpret_cast<decltype(fnInit)>(sym("piratetok_init"));
    fnShutdown=reinterpret_cast<decltype(fnShutdown)>(sym("piratetok_shutdown"));
    fnClientNew=reinterpret_cast<decltype(fnClientNew)>(sym("piratetok_client_new"));
    fnClientFree=reinterpret_cast<decltype(fnClientFree)>(sym("piratetok_client_free"));
    fnConnect=reinterpret_cast<decltype(fnConnect)>(sym("piratetok_connect"));
    fnDisconnect=reinterpret_cast<decltype(fnDisconnect)>(sym("piratetok_disconnect"));
    fnLastError=reinterpret_cast<decltype(fnLastError)>(sym("piratetok_last_error"));
    if (!fnInit || !fnShutdown || !fnClientNew || !fnClientFree || !fnConnect || !fnDisconnect || !fnLastError) {
        T0GLog::write("Chat Merger: piratetok.dll is incompatible (required exports missing)");
        unloadLocalConnector();
        return false;
    }
    runtime=fnInit();
    if (!runtime) {
        T0GLog::write("Chat Merger: local TikTok runtime failed to initialize");
        unloadLocalConnector();
        return false;
    }
    return true;
#else
    return false;
#endif
}

void TikTokChatService::unloadLocalConnector()
{
#ifdef Q_OS_WIN
    if (runtime && fnShutdown) fnShutdown(runtime);
    runtime=nullptr;
    if (libraryHandle) FreeLibrary(static_cast<HMODULE>(libraryHandle));
#endif
    libraryHandle=nullptr;
    fnInit=nullptr; fnShutdown=nullptr; fnClientNew=nullptr; fnClientFree=nullptr;
    fnConnect=nullptr; fnDisconnect=nullptr; fnLastError=nullptr;
}

void TikTokChatService::start(const QString &username)
{
    const QString normalized=normalizedUsername(username);
    if (normalized.isEmpty()) {
        stop();
        emit connectionChanged(false,"Waiting for TikTok account");
        return;
    }
    if (liveConnected && uniqueId.compare(normalized,Qt::CaseInsensitive)==0) return;

    stop();
    uniqueId=normalized;

    if (!loadLocalConnector()) {
        emit connectionChanged(false,"Local TikTok connector unavailable");
        return;
    }

    const QByteArray utf8=uniqueId.toUtf8();
    client=fnClientNew(runtime,utf8.constData());
    if (!client) {
        emit connectionChanged(false,"TikTok connector failed to initialize");
        return;
    }

    emit connectionChanged(false,"Connecting...");
    T0GLog::write("Chat Merger: connecting directly to TikTok LIVE for @"+uniqueId+" (no API key/provider)");
    const int result=fnConnect(client,&TikTokChatService::eventCallback,this);
    if (result!=0) {
        const QString detail=fnLastError ? QString::fromUtf8(fnLastError()) : QString("error %1").arg(result);
        T0GLog::write("Chat Merger: direct TikTok connect failed: "+detail);
        fnClientFree(client);
        client=nullptr;
        emit connectionChanged(false,result==3 ? "Waiting for TikTok LIVE" : "TikTok connection failed");
    }
}

void TikTokChatService::stop()
{
    liveConnected=false;
    if (client) {
        if (fnDisconnect) fnDisconnect(client);
        if (fnClientFree) fnClientFree(client);
        client=nullptr;
    }
}

void TikTokChatService::eventCallback(int type,const char *json,size_t len,void *userData)
{
    auto *self=static_cast<TikTokChatService *>(userData);
    if (!self) return;
    const QByteArray payload(json,static_cast<int>(len));
    QMetaObject::invokeMethod(self,[self,type,payload]{ self->handleEvent(type,payload); },Qt::QueuedConnection);
}

void TikTokChatService::handleEvent(int type,const QByteArray &json)
{
    if (type==kConnected) {
        liveConnected=true;
        emit connectionChanged(true,"Connected");
        T0GLog::write("Chat Merger: direct TikTok LIVE chat connected for @"+uniqueId);
        return;
    }
    if (type==kReconnecting) {
        liveConnected=false;
        emit connectionChanged(false,"Reconnecting...");
        return;
    }
    if (type==kDisconnected || type==kLiveEnded) {
        liveConnected=false;
        emit connectionChanged(false,type==kLiveEnded ? "LIVE ended" : "Disconnected");
        return;
    }
    if (type!=kChat && type!=kGift && type!=kLike && type!=kFollow && type!=kShare &&
        type!=kJoin && type!=kRoomUserSeq && type!=kSubNotify && type!=kSubscriptionNotify)
        return;

    QJsonParseError error{};
    const QJsonDocument doc=QJsonDocument::fromJson(json,&error);
    if (error.error!=QJsonParseError::NoError || !doc.isObject()) return;
    const QJsonObject o=doc.object();
    const QJsonObject user=o.value("user").toObject();

    ChatMessage message;
    message.platform=ChatPlatform::TikTok;
    message.userId=firstString(user,{"user_id","id_str","sec_uid"});
    message.username=firstString(user,{"unique_id","username"});
    message.displayName=firstString(user,{"nickname","display_name"});
    if (message.displayName.isEmpty()) message.displayName=message.username;
    const QString avatar=firstString(user,{"avatar"});
    if (!avatar.isEmpty()) message.avatarUrl=QUrl(avatar);

    switch (type) {
    case kChat:
        message.type=ChatEventType::Message;
        message.message=o.value("comment").toString();
        if (message.message.isEmpty()) return;
        break;
    case kGift: {
        message.type=ChatEventType::Gift;
        message.giftName=o.value("gift_name").toString();
        message.giftCount=o.value("repeat_count").toInt(1);
        message.diamondCount=o.value("diamond_total").toInt();
        const QJsonObject streak=o.value("streak").toObject();
        message.giftStreakActive=streak.value("is_active").toBool();
        message.giftStreakFinal=streak.value("is_final").toBool();
        const int total=streak.value("total_gift_count").toInt();
        if (total>0) message.giftCount=total;
        message.message=QString::fromUtf8("🎁 %1 ×%2").arg(message.giftName.isEmpty() ? "Gift" : message.giftName).arg(message.giftCount);
        break;
    }
    case kLike: {
        message.type=ChatEventType::Like;
        const QJsonObject stats=o.value("like_stats").toObject();
        message.likeCount=stats.value("event_like_count").toInt(o.value("like_count").toInt(1));
        message.message=QString::fromUtf8("❤️ %1 like%2").arg(message.likeCount).arg(message.likeCount==1 ? "" : "s");
        break;
    }
    case kFollow:
        message.type=ChatEventType::Follow;
        message.message=QString::fromUtf8("➕ Followed");
        break;
    case kShare:
        message.type=ChatEventType::Share;
        message.message=QString::fromUtf8("↗ Shared the LIVE");
        break;
    case kJoin:
        message.type=ChatEventType::Join;
        message.message="Joined the LIVE";
        break;
    case kSubNotify:
    case kSubscriptionNotify:
        message.type=ChatEventType::Subscription;
        message.message=QString::fromUtf8("⭐ Subscribed");
        break;
    case kRoomUserSeq:
        message.type=ChatEventType::ViewerUpdate;
        message.viewerCount=o.value("viewer_count").toInt(-1);
        message.message=message.viewerCount>=0 ? QString("%1 viewers").arg(message.viewerCount) : "Viewer count updated";
        break;
    default:
        return;
    }

    // Viewer updates are room-wide and do not require a user identity.
    if (message.type==ChatEventType::ViewerUpdate || !message.username.isEmpty())
        emit messageReceived(message);
}
