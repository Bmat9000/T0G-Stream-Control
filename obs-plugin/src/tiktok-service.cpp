#include "tiktok-service.hpp"

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

        reply->deleteLater();
        done(result);
    });
}

void TikTokService::endLive(const QString &streamId, std::function<void(bool, QString)> done)
{
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
