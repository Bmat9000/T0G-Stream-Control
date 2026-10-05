#include "chat-merger-dock.hpp"
#include <QDesktopServices>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

ChatMergerDock::ChatMergerDock(QWidget *parent) : QDockWidget("T0G Chat Merger", parent)
{
    setObjectName("T0GChatMergerDock");
    setMinimumWidth(360);
    auto *root=new QWidget(this);
    auto *rootLayout=new QVBoxLayout(root);
    rootLayout->setContentsMargins(10,10,10,10);
    rootLayout->setSpacing(8);

    auto *title=new QLabel("T0G CHAT MERGER",root);
    title->setStyleSheet("font-size: 17px; font-weight: 800;");
    rootLayout->addWidget(title);
    status=new QLabel(root);
    status->setStyleSheet("font-size: 11px;");
    rootLayout->addWidget(status);

    auto *scroll=new QScrollArea(root);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    feed=new QWidget(scroll);
    feedLayout=new QVBoxLayout(feed);
    feedLayout->setContentsMargins(0,0,0,0);
    feedLayout->setSpacing(7);
    feedLayout->addStretch(1);
    scroll->setWidget(feed);
    rootLayout->addWidget(scroll,1);

    auto *empty=new QLabel("Twitch + TikTok messages will appear here.",feed);
    empty->setObjectName("emptyChatLabel");
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setStyleSheet("color: palette(mid); padding: 24px;");
    feedLayout->insertWidget(0,empty);
    setWidget(root);
    refreshHeader();
}

void ChatMergerDock::refreshHeader()
{
    const QString twitchIcon = twitchConnected ? QString::fromUtf8("●") : QString::fromUtf8("○");
    const QString tiktokIcon = tiktokConnected ? QString::fromUtf8("●") : QString::fromUtf8("○");
    status->setText(QString("Twitch %1 %2     TikTok %3 %4")
        .arg(twitchIcon, twitchDetail, tiktokIcon, tiktokDetail));
}

void ChatMergerDock::setTwitchState(bool v, QString detail)
{
    twitchConnected=v;
    twitchDetail=detail.isEmpty() ? (v ? "Connected" : "Waiting") : detail;
    refreshHeader();
}

void ChatMergerDock::setTikTokState(bool v, QString detail)
{
    tiktokConnected=v;
    tiktokDetail=detail.isEmpty() ? (v ? "Connected" : "Waiting") : detail;
    refreshHeader();
}

void ChatMergerDock::addMessage(ChatMessage m)
{
    if (auto *empty=feed->findChild<QLabel*>("emptyChatLabel")) empty->deleteLater();

    auto *card=new QFrame(feed);
    card->setFrameShape(QFrame::StyledPanel);
    auto *v=new QVBoxLayout(card);
    v->setContentsMargins(9,7,9,7);
    v->setSpacing(3);

    auto *top=new QHBoxLayout;
    auto *name=new QPushButton(m.displayName.isEmpty()?m.username:m.displayName,card);
    name->setFlat(true);
    name->setCursor(Qt::PointingHandCursor);
    name->setStyleSheet("text-align:left; font-weight:700; padding:0; border:0;");
    auto *badge=new QLabel(m.platform==ChatPlatform::Twitch ? "TWITCH" : "TIKTOK",card);
    badge->setStyleSheet("font-size:10px; font-weight:700;");
    top->addWidget(name);
    top->addStretch();
    top->addWidget(badge);
    v->addLayout(top);

    auto *body=new QLabel(m.message,card);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(body);

    const QString user=m.username;
    const ChatPlatform platform=m.platform;
    connect(name,&QPushButton::clicked,card,[user,platform]{
        if(user.isEmpty()) return;
        const QString base=platform==ChatPlatform::Twitch ? "https://www.twitch.tv/" : "https://www.tiktok.com/@";
        QDesktopServices::openUrl(QUrl(base + QString::fromUtf8(QUrl::toPercentEncoding(user))));
    });

    feedLayout->insertWidget(feedLayout->count()-1,card);
    while(feedLayout->count()>202) {
        auto *item=feedLayout->takeAt(0);
        if(item->widget()) item->widget()->deleteLater();
        delete item;
    }
    QTimer::singleShot(0,this,[this]{
        if(auto *s=qobject_cast<QScrollArea*>(feed->parentWidget()->parentWidget()))
            s->verticalScrollBar()->setValue(s->verticalScrollBar()->maximum());
    });
}
