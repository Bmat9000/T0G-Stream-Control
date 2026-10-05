#pragma once
#include <QString>
#include <QUrl>

enum class ChatPlatform { Twitch, TikTok };

struct ChatMessage {
    ChatPlatform platform = ChatPlatform::Twitch;
    QString userId;
    QString username;
    QString displayName;
    QString message;
    QUrl avatarUrl;
};
