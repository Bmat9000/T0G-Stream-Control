#pragma once
#include <QString>
#include <QUrl>

enum class ChatPlatform { Twitch, TikTok };
enum class ChatEventType { Message, Gift, Like, Follow, Share, Join, Subscription, ViewerUpdate };

struct ChatMessage {
    ChatPlatform platform = ChatPlatform::Twitch;
    ChatEventType type = ChatEventType::Message;
    QString userId;
    QString username;
    QString displayName;
    QString message;
    QUrl avatarUrl;

    QString giftName;
    int giftCount = 0;
    int diamondCount = 0;
    bool giftStreakActive = false;
    bool giftStreakFinal = false;

    int likeCount = 0;
    int viewerCount = -1;
};
