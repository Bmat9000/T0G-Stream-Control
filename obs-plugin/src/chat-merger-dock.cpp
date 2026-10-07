#include "chat-merger-dock.hpp"
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
QWidget *viewerTile(const QString &name,QLabel **value,QWidget *parent)
{
    auto *tile=new QFrame(parent);
    tile->setStyleSheet("QFrame{background:#252936;border:1px solid #343948;border-radius:7px;}");
    auto *v=new QVBoxLayout(tile); v->setContentsMargins(9,6,9,6); v->setSpacing(1);
    auto *caption=new QLabel(name,tile);
    caption->setStyleSheet("color:#9da5b4;font-size:9px;font-weight:700;");
    *value=new QLabel("--",tile);
    (*value)->setStyleSheet("font-size:16px;font-weight:800;border:0;background:transparent;");
    v->addWidget(caption); v->addWidget(*value);
    return tile;
}
}

ChatMergerDock::ChatMergerDock(QWidget *parent) : QDockWidget("T0G Chat Merger", parent)
{
    setObjectName("T0GChatMergerDock");
    setMinimumWidth(360);
    auto *root=new QWidget(this);
    root->setStyleSheet("#chatRoot{background:#191c24;} QScrollArea{background:transparent;} QScrollBar:vertical{width:8px;}");
    root->setObjectName("chatRoot");
    auto *rootLayout=new QVBoxLayout(root);
    rootLayout->setContentsMargins(12,10,12,10); rootLayout->setSpacing(8);

    auto *titleRow=new QHBoxLayout;
    auto *title=new QLabel("T0G CHAT MERGER",root);
    title->setStyleSheet("font-size:17px;font-weight:800;");
    auto *readOnly=new QLabel("READ ONLY",root);
    readOnly->setStyleSheet("font-size:9px;font-weight:800;color:#aeb7c7;background:#292e3b;border-radius:5px;padding:3px 6px;");
    titleRow->addWidget(title); titleRow->addStretch(); titleRow->addWidget(readOnly);
    rootLayout->addLayout(titleRow);

    status=new QLabel(root); status->setStyleSheet("font-size:10px;color:#b8c0ce;");
    rootLayout->addWidget(status);

    auto *viewerRow=new QHBoxLayout; viewerRow->setSpacing(6);
    viewerRow->addWidget(viewerTile("TWITCH",&twitchViewerValue,root));
    viewerRow->addWidget(viewerTile("TIKTOK",&tiktokViewerValue,root));
    viewerRow->addWidget(viewerTile("TOTAL",&totalViewerValue,root));
    rootLayout->addLayout(viewerRow);

    likeFlushTimer=new QTimer(this); likeFlushTimer->setInterval(5000);
    connect(likeFlushTimer,&QTimer::timeout,this,&ChatMergerDock::flushLikes); likeFlushTimer->start();

    scrollArea=new QScrollArea(root); scrollArea->setWidgetResizable(true); scrollArea->setFrameShape(QFrame::NoFrame);
    feed=new QWidget(scrollArea); feedLayout=new QVBoxLayout(feed);
    feedLayout->setContentsMargins(0,0,0,0); feedLayout->setSpacing(5); feedLayout->addStretch(1);
    scrollArea->setWidget(feed); rootLayout->addWidget(scrollArea,1);

    auto *empty=new QLabel("Live chat and events will appear here.",feed);
    empty->setObjectName("emptyChatLabel"); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true);
    empty->setStyleSheet("color:#747d8e;padding:24px;");
    feedLayout->insertWidget(0,empty);
    setWidget(root); refreshHeader(); refreshViewers();
}

void ChatMergerDock::refreshHeader()
{
    const QString tw=twitchConnected ? QString::fromUtf8("●") : QString::fromUtf8("○");
    const QString tt=tiktokConnected ? QString::fromUtf8("●") : QString::fromUtf8("○");
    status->setText(QString("TWITCH %1 %2     TIKTOK %3 %4").arg(tw,twitchDetail,tt,tiktokDetail));
}
void ChatMergerDock::setTwitchState(bool v,QString d){twitchConnected=v;twitchDetail=d.isEmpty()?(v?"Connected":"Waiting"):d;refreshHeader();}
void ChatMergerDock::setTikTokState(bool v,QString d){tiktokConnected=v;tiktokDetail=d.isEmpty()?(v?"Connected":"Waiting"):d;refreshHeader();}
void ChatMergerDock::setTwitchViewers(int v){twitchViewers=v;refreshViewers();}
void ChatMergerDock::refreshViewers()
{
    twitchViewerValue->setText(twitchViewers>=0?QString::number(twitchViewers):"--");
    tiktokViewerValue->setText(tiktokViewers>=0?QString::number(tiktokViewers):"--");
    const int known=(twitchViewers>=0?twitchViewers:0)+(tiktokViewers>=0?tiktokViewers:0);
    totalViewerValue->setText((twitchViewers>=0||tiktokViewers>=0)?QString::number(known):"--");
}

void ChatMergerDock::addMessage(ChatMessage m)
{
    if(m.type==ChatEventType::ViewerUpdate){if(m.viewerCount>=0){tiktokViewers=m.viewerCount;refreshViewers();}return;}
    if(m.type==ChatEventType::Like){
        const QString key=m.userId.isEmpty()?m.username:m.userId;if(key.isEmpty())return;
        auto &p=pendingLikes[key];if(p.username.isEmpty())p=m;else p.likeCount+=qMax(1,m.likeCount);
        p.message=QString::fromUtf8("♥ %1 likes").arg(p.likeCount);return;
    }
    addEventCard(m);
}
void ChatMergerDock::flushLikes(){const auto v=pendingLikes.values();pendingLikes.clear();for(const auto &m:v)addEventCard(m);}

void ChatMergerDock::addEventCard(const ChatMessage &m)
{
    if(auto *empty=feed->findChild<QLabel*>("emptyChatLabel"))empty->deleteLater();
    auto *card=new QFrame(feed); card->setObjectName("chatCard");
    card->setStyleSheet("#chatCard{background:#20242e;border:1px solid #2d3340;border-radius:7px;} #chatCard:hover{background:#242936;border:1px solid #3b4353;}");
    auto *v=new QVBoxLayout(card);v->setContentsMargins(9,7,9,7);v->setSpacing(3);
    auto *top=new QHBoxLayout;
    QString shown=m.displayName.isEmpty()?m.username:m.displayName;if(shown.isEmpty())shown="LIVE";
    auto *name=new QPushButton(shown,card);name->setFlat(true);name->setCursor(m.username.isEmpty()?Qt::ArrowCursor:Qt::PointingHandCursor);
    name->setStyleSheet("text-align:left;font-weight:800;padding:0;border:0;background:transparent;");
    QString badge=m.platform==ChatPlatform::Twitch?"TWITCH":"TIKTOK";
    switch(m.type){case ChatEventType::Gift:badge+=" • GIFT";break;case ChatEventType::Like:badge+=" • LIKES";break;case ChatEventType::Follow:badge+=" • FOLLOW";break;case ChatEventType::Share:badge+=" • SHARE";break;case ChatEventType::Join:badge+=" • JOIN";break;case ChatEventType::Subscription:badge+=" • SUB";break;default:break;}
    auto *tag=new QLabel(badge,card);tag->setStyleSheet("font-size:9px;font-weight:800;color:#aeb7c7;");
    top->addWidget(name);top->addStretch();top->addWidget(tag);v->addLayout(top);
    auto *body=new QLabel(m.message,card);body->setWordWrap(true);body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setStyleSheet(m.type==ChatEventType::Message?"font-size:12px;":"font-size:12px;font-weight:700;");v->addWidget(body);
    const QString user=m.username;const ChatPlatform platform=m.platform;
    connect(name,&QPushButton::clicked,card,[user,platform]{if(user.isEmpty())return;const QString base=platform==ChatPlatform::Twitch?"https://www.twitch.tv/":"https://www.tiktok.com/@";QDesktopServices::openUrl(QUrl(base+QString::fromUtf8(QUrl::toPercentEncoding(user))));});

    feedLayout->insertWidget(feedLayout->count()-1,card);
    while(feedLayout->count()>202){auto *item=feedLayout->takeAt(0);if(item->widget())item->widget()->deleteLater();delete item;}

    // Keep the newest event visible even during a busy LIVE.
    QTimer::singleShot(0,this,[this]{scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());});
    // Chat is a live activity window, not permanent history. Expire each card after 60 seconds.
    QTimer::singleShot(60000,card,[card]{card->deleteLater();});
}
