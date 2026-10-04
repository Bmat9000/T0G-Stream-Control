#pragma once
#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;
class QTimer;

class TwitchService final : public QObject {
    Q_OBJECT
public:
    using Result = std::function<void(bool, QString)>;
    explicit TwitchService(QObject *parent=nullptr);
    void setClientId(const QString &id);
    QString clientId() const;
    bool connected() const;
    QString displayName() const;
    void restore(Result done = {});
    void connectDevice(Result done);
    void disconnectAccount();
    void updateChannel(const QString &title, const QString &game, Result done);

signals:
    void activationRequired(QString url, QString code);
    void accountChanged();

private:
    void pollDevice();
    void fetchIdentity(Result done);
    void searchCategory(const QString &game, std::function<void(bool,QString,QString)> done);
    void persistTokens(const QString &access, const QString &refresh);
    QString accessToken() const;
    QString refreshToken() const;

    QNetworkAccessManager *net{};
    QTimer *pollTimer{};
    QString client;
    QString deviceCode;
    QString userId;
    QString userName;
    int pollInterval = 5;
    Result pendingConnect;
};
