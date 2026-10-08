#include "chat-merger-dock.hpp"
#include <QDesktopServices>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSettings>
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
#include <QSpinBox>
#include <QTextBrowser>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <string>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QImage>
#include <algorithm>

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
    auto *gear=new QPushButton(QString::fromUtf8("⚙"),root);
    gear->setFixedSize(28,28); gear->setToolTip("Chat Merger settings");
    gear->setStyleSheet("font-size:16px;font-weight:700;padding:0;");
    connect(gear,&QPushButton::clicked,this,&ChatMergerDock::openChatSettings);
    auto *readOnly=new QLabel("READ ONLY",root);
    readOnly->setStyleSheet("font-size:9px;font-weight:800;color:#aeb7c7;background:#292e3b;border-radius:5px;padding:3px 6px;");
    titleRow->addWidget(title); titleRow->addStretch(); titleRow->addWidget(readOnly); titleRow->addWidget(gear);
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
    QSettings s("T0G","T0G Chat Merger");
    twitchViewerValue->parentWidget()->setVisible(s.value("viewers/twitch",true).toBool());
    tiktokViewerValue->parentWidget()->setVisible(s.value("viewers/tiktok",true).toBool());
    totalViewerValue->parentWidget()->setVisible(s.value("viewers/total",true).toBool());
    twitchViewerValue->setText(twitchViewers>=0?QString::number(twitchViewers):"--");
    tiktokViewerValue->setText(tiktokViewers>=0?QString::number(tiktokViewers):"--");
    const int known=(twitchViewers>=0?twitchViewers:0)+(tiktokViewers>=0?tiktokViewers:0);
    totalViewerValue->setText((twitchViewers>=0||tiktokViewers>=0)?QString::number(known):"--");
}

void ChatMergerDock::addMessage(ChatMessage m)
{
    if(!eventVisible(m)) return;
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
    if(!eventVisible(m)) return;
    QSettings settings("T0G","T0G Chat Merger");
    const int fontSize=qBound(8,settings.value("display/fontSize",12).toInt(),32);
    if(auto *empty=feed->findChild<QLabel*>("emptyChatLabel"))empty->deleteLater();
    auto *card=new QFrame(feed); card->setObjectName("chatCard");
    const QString bg=cardColor(m);
    card->setStyleSheet(QString("#chatCard{background:%1;border:1px solid #3a4050;border-radius:7px;}").arg(bg));
    auto *v=new QVBoxLayout(card);v->setContentsMargins(9,7,9,7);v->setSpacing(3);
    auto *top=new QHBoxLayout;
    QString shown=m.displayName.isEmpty()?m.username:m.displayName;if(shown.isEmpty())shown="LIVE";
    auto *name=new QPushButton(shown,card);name->setFlat(true);name->setCursor(m.username.isEmpty()?Qt::ArrowCursor:Qt::PointingHandCursor);
    name->setStyleSheet("text-align:left;font-weight:800;padding:0;border:0;background:transparent;");
    QString badge=m.platform==ChatPlatform::Twitch?"TWITCH":"TIKTOK";
    switch(m.type){case ChatEventType::Gift:badge+=" • GIFT";break;case ChatEventType::Like:badge+=" • LIKES";break;case ChatEventType::Follow:badge+=" • FOLLOW";break;case ChatEventType::Share:badge+=" • SHARE";break;case ChatEventType::Join:badge+=" • JOIN";break;case ChatEventType::Subscription:badge+=" • SUB";break;default:break;}
    if(!m.eventKey.isEmpty()) badge=(m.platform==ChatPlatform::Twitch?"TWITCH • ":"TIKTOK • ")+QString(m.eventKey).replace('_',' ').toUpper();
    if(!m.badges.isEmpty()) name->setText(shown+" ["+m.badges.join(", ")+"]");
    auto *tag=new QLabel(badge,card);tag->setStyleSheet("font-size:9px;font-weight:800;color:#aeb7c7;");
    top->addWidget(name);top->addStretch();top->addWidget(tag);v->addLayout(top);
    // Escape chat text before rendering rich content. Emote positions use Unicode code points.
    auto *body=new QTextBrowser(card); body->setFrameShape(QFrame::NoFrame);
    body->setOpenExternalLinks(false); body->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    body->setStyleSheet(QString("background:transparent;border:0;font-size:%1px;").arg(fontSize));
    const auto unicode=m.message.toUcs4();
    const std::u32string points(unicode.begin(),unicode.end());
    auto textRange=[&](int first,int count){return QString::fromUcs4(points.data()+first,count).toHtmlEscaped().replace("\n","<br>");};
    auto emotes=m.emotes; std::sort(emotes.begin(),emotes.end(),[](const ChatEmote &a,const ChatEmote &b){return a.start<b.start;});
    QString html; int cursor=0; QList<QUrl> urls;
    for(const auto &emote:emotes){
        if(emote.start<cursor || emote.end>=points.size()) continue;
        const QUrl url("https://static-cdn.jtvnw.net/emoticons/v2/"+QString::fromUtf8(QUrl::toPercentEncoding(emote.id))+"/static/dark/1.0");
        html+=textRange(cursor,emote.start-cursor);
        html+=QString("<img src=\"%1\" width=\"%2\" height=\"%2\" alt=\"%3\">").arg(url.toString().toHtmlEscaped()).arg(fontSize+8).arg(textRange(emote.start,emote.end-emote.start+1));
        cursor=emote.end+1; if(!urls.contains(url))urls.append(url);
    }
    html+=textRange(cursor,points.size()-cursor); body->setHtml(html);
    auto resizeBody=[body]{body->document()->setTextWidth(qMax(100,body->viewport()->width()));body->setFixedHeight(qMax(30,int(body->document()->size().height())+8));};
    connect(body->document()->documentLayout(),&QAbstractTextDocumentLayout::documentSizeChanged,body,[body](const QSizeF &size){body->setFixedHeight(qMax(30,int(size.height())+8));});
    v->addWidget(body); QTimer::singleShot(0,body,resizeBody);
    auto *network=urls.isEmpty()?nullptr:new QNetworkAccessManager(body);
    for(const auto &url:urls){
        auto *reply=network->get(QNetworkRequest(url));
        connect(reply,&QNetworkReply::finished,body,[body,reply,url,html,resizeBody]{
            QImage image; if(reply->error()==QNetworkReply::NoError)image.loadFromData(reply->readAll());
            if(!image.isNull()){body->document()->addResource(QTextDocument::ImageResource,url,image);body->setHtml(html);resizeBody();}
            reply->deleteLater();
        });
    }
    const QString user=m.username;const ChatPlatform platform=m.platform;
    connect(name,&QPushButton::clicked,card,[user,platform]{if(user.isEmpty())return;const QString base=platform==ChatPlatform::Twitch?"https://www.twitch.tv/":"https://www.tiktok.com/@";QDesktopServices::openUrl(QUrl(base+QString::fromUtf8(QUrl::toPercentEncoding(user))));});

    feedLayout->insertWidget(feedLayout->count()-1,card);
    while(feedLayout->count()>202){auto *item=feedLayout->takeAt(0);if(item->widget())item->widget()->deleteLater();delete item;}

    // Keep the newest event visible even during a busy LIVE.
    QTimer::singleShot(0,this,[this]{scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());});
    // Chat is a live activity window, not permanent history. Expire each card using the configured duration (60 seconds by default).
    QTimer::singleShot(qBound(5,settings.value("display/duration",60).toInt(),600)*1000,card,[card]{card->deleteLater();});
}


QString ChatMergerDock::settingKey(const ChatMessage &m) const
{
    const QString p=m.platform==ChatPlatform::Twitch?"twitch":"tiktok";
    if(!m.eventKey.isEmpty()) return p+"/"+m.eventKey;
    switch(m.type){
    case ChatEventType::Message:return p+"/chat";
    case ChatEventType::Gift:return p+"/gift";
    case ChatEventType::Like:return p+"/likes";
    case ChatEventType::Follow:return p+"/follow";
    case ChatEventType::Share:return p+"/share";
    case ChatEventType::Join:return p+"/join";
    case ChatEventType::Subscription:return p+"/sub";
    case ChatEventType::ViewerUpdate:return p+"/viewers";
    }
    return p+"/chat";
}
bool ChatMergerDock::eventVisible(const ChatMessage &m) const
{
    QSettings s("T0G","T0G Chat Merger");
    return s.value("show/"+settingKey(m),true).toBool();
}
QString ChatMergerDock::cardColor(const ChatMessage &m) const
{
    QSettings s("T0G","T0G Chat Merger");
    return s.value("color/"+settingKey(m),"#20242e").toString();
}

void ChatMergerDock::openChatSettings()
{
    QDialog d(this); d.setWindowTitle("T0G Chat Merger Settings"); d.resize(600,700);
    auto *outer=new QVBoxLayout(&d);
    auto *scroll=new QScrollArea(&d); scroll->setWidgetResizable(true); scroll->setFrameShape(QFrame::NoFrame);
    auto *content=new QWidget(scroll); auto *layout=new QVBoxLayout(content);
    QSettings s("T0G","T0G Chat Merger");

    auto *test=new QGroupBox("TikTok Testing",content); auto *testForm=new QFormLayout(test);
    auto *testUser=new QLineEdit(test); testUser->setPlaceholderText("@username");
    testUser->setText(s.value("tiktok/testUsername").toString());
    auto *note=new QLabel("Optional read-only test target. Enter a currently-LIVE TikTok username. Leave blank to use your connected TikTok account.",test);
    note->setWordWrap(true); testForm->addRow("TikTok @username",testUser); testForm->addRow("",note); layout->addWidget(test);

    struct Row{QString group,key,label;};
    const QList<Row> rows={
      {"Twitch","twitch/chat","Chat messages"},{"Twitch","twitch/follow","Followers"},{"Twitch","twitch/new_sub","New subs"},{"Twitch","twitch/resub","Resubs"},
      {"Twitch","twitch/gifted_sub","Gifted subs"},{"Twitch","twitch/bits","Bits / Cheers"},{"Twitch","twitch/raid","Raids"},
      {"TikTok","tiktok/chat","Chat messages"},{"TikTok","tiktok/gift","Gifts"},{"TikTok","tiktok/likes","Likes"},
      {"TikTok","tiktok/follow","Follows"},{"TikTok","tiktok/share","Shares"},{"TikTok","tiktok/join","Joins"},{"TikTok","tiktok/sub","Subscriptions"}};
    auto *display=new QGroupBox("Display",content); auto *displayForm=new QFormLayout(display);
    auto *fontSize=new QSpinBox(display);fontSize->setRange(8,32);fontSize->setSuffix(" px");fontSize->setValue(s.value("display/fontSize",12).toInt());
    auto *duration=new QSpinBox(display);duration->setRange(5,600);duration->setSuffix(" seconds");duration->setValue(s.value("display/duration",60).toInt());
    displayForm->addRow("Chat text size",fontSize);displayForm->addRow("Card duration",duration);layout->addWidget(display);
    auto *previewNote=new QLabel("Test buttons preview events locally without going live. Save display settings before testing. Hidden events stay hidden.",content);previewNote->setWordWrap(true);layout->addWidget(previewNote);
    QHash<QString,QGroupBox*> groups; QHash<QString,QVBoxLayout*> groupLayouts;
    for(const QString &g:{"Twitch","TikTok"}){auto *box=new QGroupBox(g,content);auto *vl=new QVBoxLayout(box);groups[g]=box;groupLayouts[g]=vl;layout->addWidget(box);}
    QList<QPair<QString,QCheckBox*>> checks;
    for(const Row &row:rows){
        auto *line=new QWidget(groups[row.group]);auto *hl=new QHBoxLayout(line);hl->setContentsMargins(0,0,0,0);
        auto *show=new QCheckBox(row.label,line);show->setChecked(s.value("show/"+row.key,true).toBool());
        auto *color=new QPushButton("Card Color",line);
        QString chosen=s.value("color/"+row.key,"#20242e").toString();
        color->setStyleSheet("background:"+chosen+";");
        connect(color,&QPushButton::clicked,&d,[color,&d,row,&s]{
            QColor initial(s.value("color/"+row.key,"#20242e").toString());
            QColor picked=QColorDialog::getColor(initial,&d,"Choose card color");
            if(picked.isValid()){s.setValue("color/"+row.key,picked.name());color->setStyleSheet("background:"+picked.name()+";");}
        });
        auto *preview=new QPushButton("Test",line);
        connect(preview,&QPushButton::clicked,&d,[this,row]{
            ChatMessage m;m.platform=row.group=="Twitch"?ChatPlatform::Twitch:ChatPlatform::TikTok;
            m.displayName="T0G Test";m.userId="t0g-preview";
            const QString key=row.key.section('/',1);
            m.type=key=="chat"?ChatEventType::Message:key=="follow"?ChatEventType::Follow:key=="likes"?ChatEventType::Like:key=="share"||key=="raid"?ChatEventType::Share:key=="join"?ChatEventType::Join:key=="gift"||key=="bits"?ChatEventType::Gift:ChatEventType::Subscription;
            if(m.platform==ChatPlatform::Twitch && key!="chat" && key!="follow")m.eventKey=key;
            m.message="Preview: "+row.label;m.likeCount=25;m.giftCount=3;m.giftName="Rose";
            if(key=="chat" && m.platform==ChatPlatform::Twitch){m.message="Hello! Kappa 😀";m.badges={"moderator","subscriber"};m.emotes.append({7,11,"25"});}
            addMessage(m);
        });
        hl->addWidget(preview);
        hl->addWidget(show);hl->addStretch();hl->addWidget(color);groupLayouts[row.group]->addWidget(line);
        checks.append({row.key,show});
    }

    auto *viewers=new QGroupBox("Viewer Dashboard",content);auto *vv=new QVBoxLayout(viewers);
    auto *showTw=new QCheckBox("Show Twitch viewers",viewers);showTw->setChecked(s.value("viewers/twitch",true).toBool());
    auto *showTt=new QCheckBox("Show TikTok viewers",viewers);showTt->setChecked(s.value("viewers/tiktok",true).toBool());
    auto *showTotal=new QCheckBox("Show Total viewers",viewers);showTotal->setChecked(s.value("viewers/total",true).toBool());
    vv->addWidget(showTw);vv->addWidget(showTt);vv->addWidget(showTotal);layout->addWidget(viewers);
    layout->addStretch();scroll->setWidget(content);outer->addWidget(scroll);

    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel|QDialogButtonBox::Reset,&d);
    connect(buttons->button(QDialogButtonBox::Reset),&QPushButton::clicked,&d,[&]{
        s.clear(); testUser->clear();fontSize->setValue(12);duration->setValue(60);
        for(auto &p:checks)p.second->setChecked(true);
        showTw->setChecked(true);showTt->setChecked(true);showTotal->setChecked(true);
    });
    connect(buttons,&QDialogButtonBox::accepted,&d,[&]{
        s.setValue("display/fontSize",fontSize->value());s.setValue("display/duration",duration->value());
        for(auto &p:checks)s.setValue("show/"+p.first,p.second->isChecked());
        s.setValue("viewers/twitch",showTw->isChecked());s.setValue("viewers/tiktok",showTt->isChecked());s.setValue("viewers/total",showTotal->isChecked());
        QString username=testUser->text().trimmed();if(username.startsWith('@'))username.remove(0,1);
        s.setValue("tiktok/testUsername",username);s.sync();emit tikTokTestUsernameChanged(username);refreshViewers();d.accept();
    });
    connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);outer->addWidget(buttons);d.exec();
}
