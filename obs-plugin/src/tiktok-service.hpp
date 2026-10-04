#pragma once
#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;

struct TikTokLiveResult {
    bool ok = false;
    QString error;
    QString server;
    QString key;
    QString streamId;
};

class TikTokService final : public QObject {
public:
    explicit TikTokService(QObject *parent = nullptr);

    void setToken(const QString &token);
    bool hasToken() const;
    QString token() const;

    static QString loadTokenFromStreamlabsDesktop(QString *error = nullptr);

    void resolveCategory(const QString &game, std::function<void(bool, QString, QString)> done);
    void startLive(const QString &title, const QString &categoryId, bool mature,
                   std::function<void(TikTokLiveResult)> done);
    void endLive(const QString &streamId, std::function<void(bool, QString)> done);

private:
    QNetworkAccessManager *network{};
    QString bearerToken;

    void applyHeaders(class QNetworkRequest &request) const;
};
