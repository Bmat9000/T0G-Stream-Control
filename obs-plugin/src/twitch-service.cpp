#include "twitch-service.hpp"
#include "credential-store.hpp"
#include "t0g-log.hpp"
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

TwitchService::TwitchService(QObject *p):QObject(p),net(new QNetworkAccessManager(this)),pollTimer(new QTimer(this)){
    connect(pollTimer,&QTimer::timeout,this,&TwitchService::pollDevice);
}
void TwitchService::setClientId(const QString&id){client=id.trimmed();}
QString TwitchService::clientId()const{return client;}
QString TwitchService::accessToken()const{return CredentialStore::read("TwitchOAuthAccess");}
QString TwitchService::refreshToken()const{return CredentialStore::read("TwitchOAuthRefresh");}
bool TwitchService::connected()const{return !userId.isEmpty()&&!accessToken().isEmpty();}
QString TwitchService::displayName()const{return userName;}
QString TwitchService::loginName()const{return userLogin;}
QString TwitchService::accountId()const{return userId;}
void TwitchService::persistTokens(const QString&a,const QString&r){QString e;CredentialStore::write("TwitchOAuthAccess",a,&e);if(!r.isEmpty())CredentialStore::write("TwitchOAuthRefresh",r,&e);}
void TwitchService::disconnectAccount(){++loginGeneration;deviceCode.clear();pendingConnect={};pollInFlight=false;pollTimer->stop();CredentialStore::remove("TwitchOAuthAccess");CredentialStore::remove("TwitchOAuthRefresh");userId.clear();userName.clear();userLogin.clear();emit accountChanged();}

void TwitchService::restore(Result done){
    if(client.isEmpty()){if(done)done(false,"Not connected");return;}
    if(accessToken().isEmpty()){refreshLogin(done);return;} fetchIdentity(done);
}
namespace {
QString twitchFailure(QNetworkReply *reply, const QJsonObject &body)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    // Never include response bodies or tokens in logs/UI.
    const QString code = body.value("error").toString();
    if (code == "access_denied") return "Twitch authorization was denied. Connect again and approve access.";
    if (code == "expired_token" || code == "invalid_device_code")
        return "The Twitch activation code expired or is invalid. Click Connect Twitch for a new code.";
    if (status == 401) return "Twitch rejected the login credentials (HTTP 401). Connect again to authorize a new login.";
    if (status == 400) return "Twitch rejected the authorization request (HTTP 400). Check the Twitch application's device-login configuration.";
    return QString("Twitch login request failed (HTTP %1, network error %2). Check your connection and try again.")
        .arg(status).arg(static_cast<int>(reply->error()));
}
}
void TwitchService::connectDevice(Result done){
    if(client.isEmpty()){done(false,"Enter the Twitch Client ID in Settings first.");return;}
    const unsigned attempt = ++loginGeneration;
    pollTimer->stop(); deviceCode.clear(); pendingConnect = {}; pollInFlight = false;
    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/device"));
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QUrlQuery q;q.addQueryItem("client_id",client);q.addQueryItem("scopes","channel:manage:broadcast chat:read channel:read:subscriptions bits:read moderator:read:followers");
    auto *r=net->post(req,q.query(QUrl::FullyEncoded).toUtf8());
    connect(r,&QNetworkReply::finished,this,[this,r,done,attempt]{
        const auto o=QJsonDocument::fromJson(r->readAll()).object();r->deleteLater();
        if (attempt != loginGeneration) return;
        if (r->error() != QNetworkReply::NoError) { done(false, twitchFailure(r, o)); return; }
        deviceCode=o.value("device_code").toString(); const auto code=o.value("user_code").toString();
        const auto uri=o.value("verification_uri").toString(); pollInterval=qMax(2,o.value("interval").toInt(5));
        if(deviceCode.isEmpty()||uri.isEmpty()){done(false,"Twitch did not return a device authorization code.");return;}
        deviceExpiresIn=qMax(1,o.value("expires_in").toInt(600)); deviceStarted.start();
        pendingConnect=done;
        // Poll before showing activation instructions; a modal dialog must not
        // postpone token retrieval until its OK button is clicked.
        pollTimer->start(pollInterval*1000);
        QDesktopServices::openUrl(QUrl(uri)); emit activationRequired(uri,code);
    });
}
void TwitchService::pollDevice(){
    if(deviceCode.isEmpty()){pollTimer->stop();return;}
    if (pollInFlight) return;
    if (deviceStarted.elapsed() >= static_cast<qint64>(deviceExpiresIn) * 1000) {
        pollTimer->stop(); deviceCode.clear();
        auto cb=pendingConnect; pendingConnect={};
        if(cb) cb(false,"Twitch authorization timed out. Click Connect Twitch for a new code.");
        return;
    }
    const unsigned attempt = loginGeneration;
    pollInFlight = true;
    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/token"));
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QUrlQuery q;q.addQueryItem("client_id",client);
    q.addQueryItem("device_code",deviceCode);q.addQueryItem("grant_type","urn:ietf:params:oauth:grant-type:device_code");
    auto *r=net->post(req,q.query(QUrl::FullyEncoded).toUtf8());
    connect(r,&QNetworkReply::finished,this,[this,r,attempt]{
        const auto o=QJsonDocument::fromJson(r->readAll()).object();r->deleteLater();
        if (attempt != loginGeneration) return;
        pollInFlight = false;
        const QString error=o.value("error").toString();
        // Twitch's device endpoint can put its pending condition in message.
        const QString message=o.value("message").toString();
        if(error=="authorization_pending" || message=="authorization_pending") return;
        if(error=="slow_down" || message=="slow_down") {
            pollInterval += 5; pollTimer->setInterval(pollInterval*1000); return;
        }
        const auto a=o.value("access_token").toString();
        if(r->error()!=QNetworkReply::NoError || a.isEmpty()) {
            pollTimer->stop(); deviceCode.clear();
            auto cb=pendingConnect; pendingConnect={};
            const QString failure=twitchFailure(r,o);
            T0GLog::write(failure, LOG_WARNING);
            if(cb) cb(false,failure);
            return;
        }
        pollTimer->stop();deviceCode.clear();
        QString storageError;
        if (!CredentialStore::write("TwitchOAuthAccess",a,&storageError) ||
            (!o.value("refresh_token").toString().isEmpty() &&
             !CredentialStore::write("TwitchOAuthRefresh",o.value("refresh_token").toString(),&storageError))) {
            auto cb=pendingConnect;pendingConnect={};
            if(cb)cb(false,"Twitch authorized successfully, but Windows could not save the login credentials.");
            return;
        }
        fetchIdentity([this,attempt](bool ok,QString msg){
            if(attempt != loginGeneration) return;
            auto cb=pendingConnect;pendingConnect={};if(cb)cb(ok,msg);
        });
    });
}
void TwitchService::refreshLogin(Result done)
{
    const QString savedRefresh = refreshToken();
    if (savedRefresh.isEmpty()) { if(done) done(false,"Connect Twitch in Settings to authorize this account."); return; }
    const unsigned attempt = loginGeneration;
    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/token"));
    req.setTransferTimeout(15000);
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QUrlQuery q;
    q.addQueryItem("client_id",client);
    q.addQueryItem("grant_type","refresh_token");
    q.addQueryItem("refresh_token",savedRefresh);
    auto *reply = net->post(req,q.query(QUrl::FullyEncoded).toUtf8());
    connect(reply,&QNetworkReply::finished,this,[this,reply,done,attempt] {
        const auto body = QJsonDocument::fromJson(reply->readAll()).object();
        reply->deleteLater();
        if (attempt != loginGeneration) return;
        const QString access = body.value("access_token").toString();
        if (reply->error() != QNetworkReply::NoError || access.isEmpty()) {
            // Keep saved credentials on temporary network/service failures.
            if(done) done(false,"Could not renew Twitch login. Check your connection; if access was revoked, connect Twitch again.");
            return;
        }
        QString error;
        const QString refresh = body.value("refresh_token").toString();
        // Save a rotated refresh token first so it survives an interrupted write.
        if ((!refresh.isEmpty() && !CredentialStore::write("TwitchOAuthRefresh",refresh,&error)) ||
            !CredentialStore::write("TwitchOAuthAccess",access,&error)) {
            if(done) done(false,"Windows could not save the renewed Twitch login.");
            return;
        }
        fetchIdentity(done,false);
    });
}
void TwitchService::fetchIdentity(Result done, bool allowRefresh){
    const unsigned attempt = loginGeneration;
    QNetworkRequest req(QUrl("https://api.twitch.tv/helix/users"));req.setTransferTimeout(15000);req.setRawHeader("Client-Id",client.toUtf8());req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());
    auto*r=net->get(req);connect(r,&QNetworkReply::finished,this,[this,r,done,allowRefresh,attempt]{
        if (attempt != loginGeneration) { r->deleteLater(); return; }
        if (allowRefresh && r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            r->deleteLater(); refreshLogin(done); return;
        }
        auto a=QJsonDocument::fromJson(r->readAll()).object().value("data").toArray();r->deleteLater();
        if(a.isEmpty()){T0GLog::write("Twitch login identity lookup failed", LOG_WARNING);if(done)done(false,"Twitch authorization finished, but account validation failed. Check your connection and reconnect Twitch.");return;}auto o=a.first().toObject();userId=o.value("id").toString();userName=o.value("display_name").toString();userLogin=o.value("login").toString();emit accountChanged();if(done)done(true,userName);});
}
void TwitchService::searchCategory(const QString&game,std::function<void(bool,QString,QString)> done){
    if(game.trimmed().isEmpty()){done(true,{},{});return;} QUrl u("https://api.twitch.tv/helix/search/categories");QUrlQuery q;q.addQueryItem("query",game);u.setQuery(q);
    QNetworkRequest req(u);req.setRawHeader("Client-Id",client.toUtf8());req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());
    auto*r=net->get(req);connect(r,&QNetworkReply::finished,this,[r,done]{auto a=QJsonDocument::fromJson(r->readAll()).object().value("data").toArray();r->deleteLater();if(a.isEmpty()){done(false,{},"Twitch category was not found.");return;}done(true,a.first().toObject().value("id").toString(),{});});
}
void TwitchService::updateChannel(const QString&title,const QString&game,Result done){
    if(!connected()){done(false,"Connect Twitch in Settings first.");return;}
    searchCategory(game,[this,title,done](bool ok,QString gid,QString err){if(!ok){done(false,err);return;}QUrl u("https://api.twitch.tv/helix/channels");QUrlQuery q;q.addQueryItem("broadcaster_id",userId);u.setQuery(q);
        QNetworkRequest req(u);req.setRawHeader("Client-Id",client.toUtf8());req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());req.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
        QJsonObject b;b["title"]=title;if(!gid.isEmpty())b["game_id"]=gid;auto*r=net->sendCustomRequest(req,"PATCH",QJsonDocument(b).toJson(QJsonDocument::Compact));
        connect(r,&QNetworkReply::finished,this,[r,done]{const bool ok=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==204;const auto e=QString::fromUtf8(r->readAll());r->deleteLater();done(ok,ok?"Twitch stream info updated.":("Twitch update failed: "+e));});});
}
