#pragma once
#include <QString>

class SessionState {
public:
    static void setTikTokCredentials(const QString &server, const QString &key, const QString &streamId);
    static QString tikTokServer();
    static QString tikTokKey();
    static QString tikTokStreamId();
    static void clearTikTok();
};
