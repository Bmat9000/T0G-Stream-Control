#include "tiktok-service.hpp"
#include "t0g-log.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QRandomGenerator>

namespace {
constexpr auto kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) StreamlabsDesktop/1.20.4 Chrome/122.0.6261.156 "
    "Electron/29.3.1 Safari/537.36";

QHttpPart textPart(const char *name, const QString &value)
{
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QString("form-data; name=\"%1\"").arg(QString::fromUtf8(name)));
    part.setBody(value.toUtf8());
    return part;
}
}

TikTokService::TikTokService(QObject *parent) : QObject(parent)
{
    network = new QNetworkAccessManager(this);
}

void TikTokService::setToken(const QString &token)
{
    bearerToken = token.trimmed();
}

bool TikTokService::hasToken() const
{
    return !bearerToken.isEmpty();
}

QString TikTokService::token() const
{
    return bearerToken;
}

void TikTokService::applyHeaders(QNetworkRequest &request) const
{
    request.setRawHeader("User-Agent", kUserAgent);
    request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
}

QString TikTokService::loadTokenFromStreamlabsDesktop(QString *error)
{
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) {
        if (error) *error = "APPDATA is not available.";
        return {};
    }
    const QString levelDbPath = QDir(appData).filePath("slobs-client/Local Storage/leveldb");
#elif defined(Q_OS_MACOS)
    const QString levelDbPath = QDir::home().filePath(
        "Library/Application Support/slobs-client/Local Storage/leveldb");
#else
    if (error) *error = "Automatic Streamlabs token loading is currently supported on Windows/macOS.";
    return {};
#endif

    QDir dir(levelDbPath);
    if (!dir.exists()) {
        if (error) *error = "Streamlabs Desktop data was not found. Sign into TikTok in Streamlabs first.";
        return {};
    }

    QFileInfoList logs = dir.entryInfoList({"*.log"}, QDir::Files, QDir::Time);
    const QRegularExpression re(QStringLiteral("\\\"apiToken\\\":\\\"([a-fA-F0-9]+)\\\""));

    for (const QFileInfo &info : logs) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;

        const QString contents = QString::fromUtf8(file.readAll());
        auto it = re.globalMatch(contents);
        QString last;
        while (it.hasNext())
            last = it.next().captured(1);
        if (!last.isEmpty())
            return last;
    }

    if (error) *error = "No Streamlabs API token was found. Make sure Streamlabs is signed into TikTok.";
    return {};
}


void TikTokService::loadTokenFromWeb(std::function<void(bool, QString, QString)> done)
{
    T0GLog::write("Streamlabs web login: starting local callback listener");
    auto *server = new QTcpServer(this);
    if (!server->listen(QHostAddress::LocalHost, 0)) {
        T0GLog::write("Streamlabs web login: local callback listener FAILED", LOG_ERROR);
        done(false, {}, "Could not start the local Streamlabs login callback.");
        server->deleteLater();
        return;
    }

    QByteArray random(64, Qt::Uninitialized);
    for (qsizetype i = 0; i < random.size(); ++i)
        random[i] = char(QRandomGenerator::global()->generate() & 0xff);
    const QString verifier = QString::fromLatin1(random.toHex());
    QByteArray challenge = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256)
                               .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    QUrl login("https://streamlabs.com/slobs/login");
    QUrlQuery q;
    q.addQueryItem("skip_splash", "true");
    q.addQueryItem("external", "electron");
    q.addQueryItem("tiktok", "");
    q.addQueryItem("force_verify", "");
    q.addQueryItem("origin", "slobs");
    q.addQueryItem("port", QString::number(server->serverPort()));
    q.addQueryItem("code_challenge", QString::fromLatin1(challenge));
    q.addQueryItem("code_flow", "true");
    login.setQuery(q);

    connect(server, &QTcpServer::newConnection, this, [this, server, verifier, done = std::move(done)]() mutable {
        auto *socket = server->nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, server, socket, verifier, done = std::move(done)]() mutable {
            const QByteArray request = socket->readAll();
            const QByteArray firstLine = request.left(request.indexOf("\r\n"));
            const QList<QByteArray> parts = firstLine.split(' ');
            QUrl callback(parts.size() > 1 ? QString::fromUtf8(parts[1]) : QString());
            QUrlQuery params(callback);
            const QString code = params.queryItemValue("code");
            const bool success = params.queryItemValue("success") == "true" && !code.isEmpty();

            const QByteArray body = success
                ? "<h2>Authentication successful. You can close this tab.</h2>"
                : "<h2>Authentication failed. Return to OBS and try again.</h2>";
            socket->write("HTTP/1.1 " + QByteArray(success ? "200 OK" : "400 Bad Request") +
                          "\r\nContent-Type: text/html\r\nContent-Length: " +
                          QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
            server->close();
            server->deleteLater();

            if (!success) {
                done(false, {}, "Streamlabs login did not return an authorization code.");
                return;
            }

            QUrl url("https://streamlabs.com/api/v5/slobs/auth/data");
            QUrlQuery exchange;
            exchange.addQueryItem("code_verifier", verifier);
            exchange.addQueryItem("code", code);
            url.setQuery(exchange);
            QNetworkRequest req(url);
            req.setRawHeader("User-Agent", kUserAgent);
            req.setRawHeader("Accept", "*/*");
            auto *reply = network->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, done = std::move(done)]() mutable {
                const QByteArray body = reply->readAll();
                if (reply->error() != QNetworkReply::NoError) {
                    const QString err = reply->errorString();
                    reply->deleteLater();
                    done(false, {}, err);
                    return;
                }
                const QJsonObject root = QJsonDocument::fromJson(body).object();
                const QString token = root.value("data").toObject().value("oauth_token").toString();
                reply->deleteLater();
                if (!root.value("success").toBool() || token.isEmpty()) {
                    done(false, {}, "Streamlabs did not return a TikTok OAuth token.");
                    return;
                }
                setToken(token);
                T0GLog::write("Streamlabs web login: token received successfully (token hidden)");
                done(true, token, {});
            });
        });
    });

    T0GLog::write(QString("Streamlabs web login: opening browser; callback port=%1").arg(server->serverPort()));
    QDesktopServices::openUrl(login);
}

void TikTokService::getAccountInfo(std::function<void(TikTokAccountInfo)> done)
{
    T0GLog::write("TikTok API: requesting account info");
    if (!hasToken()) {
        TikTokAccountInfo result; result.error = "TikTok/Streamlabs is not connected."; done(result); return;
    }
    QNetworkRequest request(QUrl("https://streamlabs.com/api/v5/slobs/tiktok/info"));
    applyHeaders(request);
    auto *reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
        TikTokAccountInfo result;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString();
        } else {
            const QJsonObject root = QJsonDocument::fromJson(body).object();
            result.username = root.value("user").toObject().value("username").toString("Unknown");
            result.status = root.value("application_status").toObject().value("status").toString("Unknown");
            result.canGoLive = root.value("can_be_live").toBool(false);
            result.ok = true;
        }
        T0GLog::write(QString("TikTok API: account info %1; canGoLive=%2; error=%3").arg(result.ok ? "OK" : "FAILED", result.canGoLive ? "true" : "false", result.error.isEmpty() ? "<none>" : result.error), result.ok ? LOG_INFO : LOG_ERROR);
        reply->deleteLater();
        done(result);
    });
}

void TikTokService::searchCategories(const QString &game,
    std::function<void(bool, QVector<TikTokCategory>, QString)> done)
{
    if (!hasToken()) { done(false, {}, "TikTok/Streamlabs is not connected."); return; }
    if (game.trimmed().isEmpty()) { done(true, {}, {}); return; }

    QUrl url("https://streamlabs.com/api/v5/slobs/tiktok/info");
    QUrlQuery query; query.addQueryItem("category", game.left(25)); url.setQuery(query);
    QNetworkRequest request(url); applyHeaders(request);
    auto *reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
        QVector<TikTokCategory> out;
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString err=reply->errorString(); reply->deleteLater(); done(false, {}, err); return;
        }
        const QJsonArray categories=QJsonDocument::fromJson(body).object().value("categories").toArray();
        for (const auto &v : categories) {
            const auto o=v.toObject();
            out.push_back({o.value("full_name").toString(), o.value("game_mask_id").toString()});
        }
        out.push_back({"Other", ""});
        reply->deleteLater(); done(true, out, {});
    });
}

void TikTokService::resolveCategory(const QString &game,
                                    std::function<void(bool, QString, QString)> done)
{
    if (!hasToken()) {
        done(false, {}, "TikTok/Streamlabs is not connected.");
        return;
    }

    QUrl url("https://streamlabs.com/api/v5/slobs/tiktok/info");
    QUrlQuery query;
    query.addQueryItem("category", game.left(25));
    url.setQuery(query);

    QNetworkRequest request(url);
    applyHeaders(request);
    auto *reply = network->get(request);

    connect(reply, &QNetworkReply::finished, this, [reply, game, done = std::move(done)]() mutable {
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            done(false, {}, err);
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(body).object();
        const QJsonArray categories = root.value("categories").toArray();
        if (categories.isEmpty()) {
            reply->deleteLater();
            done(true, {}, {});
            return;
        }

        QString chosenId;
        const QString wanted = game.trimmed();
        for (const QJsonValue &value : categories) {
            const QJsonObject obj = value.toObject();
            if (obj.value("full_name").toString().compare(wanted, Qt::CaseInsensitive) == 0) {
                chosenId = obj.value("game_mask_id").toString();
                break;
            }
        }
        if (chosenId.isEmpty())
            chosenId = categories.first().toObject().value("game_mask_id").toString();

        reply->deleteLater();
        done(true, chosenId, {});
    });
}

void TikTokService::startLive(const QString &title, const QString &categoryId, bool mature,
                              std::function<void(TikTokLiveResult)> done)
{
    T0GLog::write(QString("TikTok API: creating LIVE; titleLen=%1 categorySet=%2 mature=%3").arg(title.size()).arg(categoryId.isEmpty() ? "false" : "true").arg(mature ? "true" : "false"));
    if (!hasToken()) {
        done({false, "TikTok/Streamlabs is not connected.", {}, {}, {}});
        return;
    }

    QNetworkRequest request(QUrl("https://streamlabs.com/api/v5/slobs/tiktok/stream/start"));
    applyHeaders(request);

    auto *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    multi->append(textPart("title", title));
    multi->append(textPart("device_platform", "win32"));
    multi->append(textPart("category", categoryId));
    multi->append(textPart("audience_type", mature ? "1" : "0"));

    auto *reply = network->post(request, multi);
    multi->setParent(reply);

    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
        TikTokLiveResult result;
        const QByteArray body = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString();
        } else {
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            result.server = obj.value("rtmp").toString();
            result.key = obj.value("key").toString();
            const QJsonValue idValue = obj.value("id");
            result.streamId = idValue.isString() ? idValue.toString()
                                                 : QString::number(idValue.toVariant().toLongLong());
            result.ok = !result.server.isEmpty() && !result.key.isEmpty();
            if (!result.ok) {
                result.error = obj.value("message").toString();
                if (result.error.isEmpty())
                    result.error = obj.value("data").toObject().value("message").toString();
                if (result.error.isEmpty())
                    result.error = "Streamlabs did not return an RTMP destination.";
            }
        }

        T0GLog::write(QString("TikTok API: create LIVE %1; server=%2; streamIdSet=%3; key=%4; error=%5").arg(result.ok ? "OK" : "FAILED", result.server.isEmpty() ? "<missing>" : result.server, result.streamId.isEmpty() ? "false" : "true", result.key.isEmpty() ? "<missing>" : "<hidden>", result.error.isEmpty() ? "<none>" : result.error), result.ok ? LOG_INFO : LOG_ERROR);
        reply->deleteLater();
        done(result);
    });
}

void TikTokService::endLive(const QString &streamId, std::function<void(bool, QString)> done)
{
    T0GLog::write(QString("TikTok API: ending LIVE; streamIdSet=%1").arg(streamId.isEmpty() ? "false" : "true"));
    if (!hasToken() || streamId.isEmpty()) {
        done(false, "No active TikTok LIVE session.");
        return;
    }

    QNetworkRequest request(QUrl(
        QString("https://streamlabs.com/api/v5/slobs/tiktok/stream/%1/end").arg(streamId)));
    applyHeaders(request);
    auto *reply = network->post(request, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() mutable {
        const QByteArray body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            done(false, err);
            return;
        }
        const bool success = QJsonDocument::fromJson(body).object().value("success").toBool(false);
        reply->deleteLater();
        done(success, success ? QString() : QString("Streamlabs did not confirm the stream ended."));
    });
}
