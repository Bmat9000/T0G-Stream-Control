#pragma once
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QList>

struct ChatEmote { int start=0; int end=0; QString id; };

enum class ChatPlatform { Twitch, TikTok };
enum class ChatEventType { Message, Gift, Like, Follow, Share, Join, Subscription, ViewerUpdate };

struct ChatMessage {
    ChatPlatform platform = ChatPlatform::Twitch;
    ChatEventType type = ChatEventType::Message;
    QString userId;
    QString username;
    QString displayName;
    QString message;
    QString eventKey; // optional subtype: new_sub, resub, gifted_sub, bits, raid
    QUrl avatarUrl;
    QStringList badges;
    QList<ChatEmote> emotes;

    QString giftName;
    int giftCount = 0;
    int diamondCount = 0;
    bool giftStreakActive = false;
    bool giftStreakFinal = false;

    int likeCount = 0;
    int viewerCount = -1;
};
