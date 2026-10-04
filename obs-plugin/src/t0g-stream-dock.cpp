#include "t0g-stream-dock.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

T0GStreamDock::T0GStreamDock(QWidget *parent) : QDockWidget("T0G Stream Control", parent)
{
    setObjectName("T0GStreamControlDock");
    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);

    auto *brand = new QLabel("T0G STREAM CONTROL", root);
    brand->setStyleSheet("font-size: 18px; font-weight: 700;");
    layout->addWidget(brand);

    auto *form = new QFormLayout;
    titleEdit = new QLineEdit(root);
    titleEdit->setPlaceholderText("Stream title");
    gameEdit = new QLineEdit(root);
    gameEdit->setPlaceholderText("Game / category");
    audienceBox = new QComboBox(root);
    audienceBox->addItems({"Everyone", "Mature / 18+"});
    form->addRow("Title", titleEdit);
    form->addRow("Game", gameEdit);
    form->addRow("TikTok audience", audienceBox);
    layout->addLayout(form);

    auto *platforms = new QGroupBox("Platforms", root);
    auto *platformLayout = new QVBoxLayout(platforms);
    twitchEnabled = new QCheckBox("Twitch", platforms);
    tiktokEnabled = new QCheckBox("TikTok", platforms);
    twitchEnabled->setChecked(true);
    tiktokEnabled->setChecked(true);
    twitchStatus = new QLabel("Twitch: Not connected", platforms);
    tiktokStatus = new QLabel("TikTok: Not connected", platforms);
    platformLayout->addWidget(twitchEnabled);
    platformLayout->addWidget(twitchStatus);
    platformLayout->addWidget(tiktokEnabled);
    platformLayout->addWidget(tiktokStatus);
    layout->addWidget(platforms);

    updateButton = new QPushButton("UPDATE STREAM INFO", root);
    goLiveButton = new QPushButton("GO LIVE", root);
    goLiveButton->setMinimumHeight(42);
    layout->addWidget(updateButton);
    layout->addWidget(goLiveButton);
    layout->addStretch();

    connect(twitchEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });
    connect(tiktokEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });
    updateReadyState();
    setWidget(root);
}

void T0GStreamDock::updateReadyState()
{
    const bool anyPlatform = twitchEnabled->isChecked() || tiktokEnabled->isChecked();
    updateButton->setEnabled(anyPlatform);
    goLiveButton->setEnabled(anyPlatform);
}
