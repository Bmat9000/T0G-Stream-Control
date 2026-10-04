#include "session-state.hpp"
#include <QSettings>
static QSettings state(){ return QSettings("T0G","T0G Stream Control Session"); }
void SessionState::setTikTokCredentials(const QString&s,const QString&k,const QString&id){auto q=state();q.setValue("tiktok/server",s);q.setValue("tiktok/key",k);q.setValue("tiktok/id",id);q.sync();}
QString SessionState::tikTokServer(){return state().value("tiktok/server").toString();}
QString SessionState::tikTokKey(){return state().value("tiktok/key").toString();}
QString SessionState::tikTokStreamId(){return state().value("tiktok/id").toString();}
void SessionState::clearTikTok(){auto q=state();q.remove("tiktok");q.sync();}
