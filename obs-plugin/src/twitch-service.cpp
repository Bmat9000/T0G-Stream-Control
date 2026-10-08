#include "twitch-service.hpp"
#include "credential-store.hpp"
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
void TwitchService::disconnectAccount(){pollTimer->stop();CredentialStore::remove("TwitchOAuthAccess");CredentialStore::remove("TwitchOAuthRefresh");userId.clear();userName.clear();userLogin.clear();emit accountChanged();}

void TwitchService::restore(Result done){
    if(client.isEmpty()||accessToken().isEmpty()){if(done)done(false,"Not connected");return;} fetchIdentity(done);
}
void TwitchService::connectDevice(Result done){
    if(client.isEmpty()){done(false,"Enter the Twitch Client ID in Settings first.");return;}
    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/device"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QUrlQuery q;q.addQueryItem("client_id",client);q.addQueryItem("scopes","channel:manage:broadcast chat:read channel:read:subscriptions bits:read moderator:read:followers");
    auto *r=net->post(req,q.query(QUrl::FullyEncoded).toUtf8());
    connect(r,&QNetworkReply::finished,this,[this,r,done]{
        const auto o=QJsonDocument::fromJson(r->readAll()).object();r->deleteLater();
        deviceCode=o.value("device_code").toString(); const auto code=o.value("user_code").toString();
        const auto uri=o.value("verification_uri").toString(); pollInterval=qMax(2,o.value("interval").toInt(5));
        if(deviceCode.isEmpty()||uri.isEmpty()){done(false,"Twitch did not return a device authorization code.");return;}
        pendingConnect=done; QDesktopServices::openUrl(QUrl(uri)); emit activationRequired(uri,code);
        pollTimer->start(pollInterval*1000);
    });
}
void TwitchService::pollDevice(){
    if(deviceCode.isEmpty()){pollTimer->stop();return;}
    QNetworkRequest req(QUrl("https://id.twitch.tv/oauth2/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    QUrlQuery q;q.addQueryItem("client_id",client);q.addQueryItem("scope","channel:manage:broadcast chat:read channel:read:subscriptions bits:read moderator:read:followers");
    q.addQueryItem("device_code",deviceCode);q.addQueryItem("grant_type","urn:ietf:params:oauth:grant-type:device_code");
    auto *r=net->post(req,q.query(QUrl::FullyEncoded).toUtf8());
    connect(r,&QNetworkReply::finished,this,[this,r]{
        const auto o=QJsonDocument::fromJson(r->readAll()).object();r->deleteLater();
        const auto a=o.value("access_token").toString();
        if(a.isEmpty())return;
        pollTimer->stop();deviceCode.clear();persistTokens(a,o.value("refresh_token").toString());
        fetchIdentity([this](bool ok,QString msg){auto cb=pendingConnect;pendingConnect={};if(cb)cb(ok,msg);});
    });
}
void TwitchService::fetchIdentity(Result done){
    QNetworkRequest req(QUrl("https://api.twitch.tv/helix/users"));req.setRawHeader("Client-Id",client.toUtf8());req.setRawHeader("Authorization",("Bearer "+accessToken()).toUtf8());
    auto*r=net->get(req);connect(r,&QNetworkReply::finished,this,[this,r,done]{auto a=QJsonDocument::fromJson(r->readAll()).object().value("data").toArray();r->deleteLater();
        if(a.isEmpty()){if(done)done(false,"Twitch login could not be validated.");return;}auto o=a.first().toObject();userId=o.value("id").toString();userName=o.value("display_name").toString();userLogin=o.value("login").toString();emit accountChanged();if(done)done(true,userName);});
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
