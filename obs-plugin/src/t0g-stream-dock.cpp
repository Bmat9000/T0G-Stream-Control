#include "t0g-stream-dock.hpp"

#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

T0GStreamDock::T0GStreamDock(QWidget *parent) : QDockWidget("T0G Stream Control", parent)
{
    setObjectName("T0GStreamControlDock");
    setMinimumWidth(340);

    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);

    auto *brand = new QLabel("T0G STREAM CONTROL", root);
    brand->setStyleSheet("font-size: 18px; font-weight: 700;");
    layout->addWidget(brand);

    auto *subtitle = new QLabel("Twitch + TikTok control from inside OBS", root);
    subtitle->setStyleSheet("color: palette(mid);");
    layout->addWidget(subtitle);

    auto *form = new QFormLayout;
    titleEdit = new QLineEdit(root);
    titleEdit->setPlaceholderText("Ranked R6 | Road to Champ");
    gameEdit = new QLineEdit(root);
    gameEdit->setPlaceholderText("Rainbow Six Siege");
    audienceBox = new QComboBox(root);
    audienceBox->addItems({"Everyone", "Mature / 18+"});
    form->addRow("Title", titleEdit);
    form->addRow("Game", gameEdit);
    form->addRow("TikTok audience", audienceBox);
    layout->addLayout(form);

    auto *platforms = new QGroupBox("Platforms", root);
    auto *platformLayout = new QVBoxLayout(platforms);

    twitchEnabled = new QCheckBox("Twitch (normal OBS stream)", platforms);
    twitchEnabled->setChecked(true);
    twitchStatus = new QLabel("Twitch: Ready when OBS is configured", platforms);

    tiktokEnabled = new QCheckBox("TikTok (secondary RTMP output)", platforms);
    tiktokEnabled->setChecked(true);
    tiktokStatus = new QLabel("TikTok: Not connected", platforms);

    loadTikTokButton = new QPushButton("LOAD TIKTOK FROM STREAMLABS", platforms);
    loadTikTokButton->setToolTip("Loads your existing Streamlabs TikTok session locally. The token is not saved by T0G.");

    platformLayout->addWidget(twitchEnabled);
    platformLayout->addWidget(twitchStatus);
    platformLayout->addSpacing(6);
    platformLayout->addWidget(tiktokEnabled);
    platformLayout->addWidget(tiktokStatus);
    platformLayout->addWidget(loadTikTokButton);
    layout->addWidget(platforms);

    updateButton = new QPushButton("UPDATE STREAM INFO", root);
    updateButton->setToolTip("TikTok uses these fields when T0G creates the LIVE session. Twitch metadata API wiring is the next milestone.");

    auto *buttons = new QHBoxLayout;
    goLiveButton = new QPushButton("GO LIVE", root);
    goLiveButton->setMinimumHeight(42);
    endLiveButton = new QPushButton("END LIVE", root);
    endLiveButton->setMinimumHeight(42);
    endLiveButton->setEnabled(false);
    buttons->addWidget(goLiveButton);
    buttons->addWidget(endLiveButton);

    layout->addWidget(updateButton);
    layout->addLayout(buttons);
    layout->addStretch();

    connect(loadTikTokButton, &QPushButton::clicked, this, [this] { loadTikTokToken(); });
    connect(goLiveButton, &QPushButton::clicked, this, [this] { startSelectedPlatforms(); });
    connect(endLiveButton, &QPushButton::clicked, this, [this] { stopSelectedPlatforms(); });
    connect(updateButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "T0G Stream Control",
                                 "Your title/game settings are ready. TikTok will apply them when you go live. Twitch metadata sync is not wired yet.");
    });
    connect(twitchEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });
    connect(tiktokEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });

    updateReadyState();
    setWidget(root);
}

void T0GStreamDock::setBusy(bool value)
{
    busy = value;
    loadTikTokButton->setEnabled(!busy);
    updateReadyState();
}

void T0GStreamDock::setTikTokStatus(const QString &text)
{
    tiktokStatus->setText("TikTok: " + text);
}

void T0GStreamDock::setTwitchStatus(const QString &text)
{
    twitchStatus->setText("Twitch: " + text);
}

void T0GStreamDock::loadTikTokToken()
{
    QString error;
    const QString token = TikTokService::loadTokenFromStreamlabsDesktop(&error);
    if (token.isEmpty()) {
        setTikTokStatus("Not connected");
        QMessageBox::warning(this, "TikTok Connection", error);
        return;
    }

    tiktok.setToken(token);
    setTikTokStatus("Connected through Streamlabs");
    updateReadyState();
}

void T0GStreamDock::startSelectedPlatforms()
{
    if (busy)
        return;

    if (titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing title", "Enter a stream title first.");
        return;
    }

    setBusy(true);

    if (twitchEnabled->isChecked()) {
        if (!obs_frontend_streaming_active()) {
            setTwitchStatus("Starting...");
            obs_frontend_streaming_start();
        } else {
            setTwitchStatus("LIVE");
        }
    }

    if (tiktokEnabled->isChecked()) {
        if (!tiktok.hasToken()) {
            setBusy(false);
            QMessageBox::warning(this, "TikTok not connected",
                                 "Click LOAD TIKTOK FROM STREAMLABS first.");
            return;
        }
        startTikTok();
        return;
    }

    setBusy(false);
    endLiveButton->setEnabled(true);
}

void T0GStreamDock::startTikTok()
{
    setTikTokStatus("Resolving category...");

    tiktok.resolveCategory(gameEdit->text().trimmed(),
        [this](bool ok, QString categoryId, QString error) {
            if (!ok) {
                setTikTokStatus("Category lookup failed");
                setBusy(false);
                QMessageBox::critical(this, "TikTok", error);
                return;
            }

            setTikTokStatus("Creating LIVE...");
            tiktok.startLive(titleEdit->text().trimmed(), categoryId,
                             audienceBox->currentIndex() == 1,
                [this](TikTokLiveResult result) {
                    if (!result.ok) {
                        setTikTokStatus("Failed to create LIVE");
                        setBusy(false);
                        QMessageBox::critical(this, "TikTok LIVE", result.error);
                        return;
                    }

                    activeTikTokStreamId = result.streamId;
                    setTikTokStatus("Configuring OBS output...");

                    QString outputError;
                    if (!tiktokOutput.configure(result.server, result.key, &outputError)) {
                        setTikTokStatus("Output setup failed");
                        tiktok.endLive(activeTikTokStreamId, [](bool, QString) {});
                        activeTikTokStreamId.clear();
                        setBusy(false);
                        QMessageBox::critical(this, "TikTok Output", outputError);
                        return;
                    }

                    if (!tiktokOutput.start(&outputError)) {
                        setTikTokStatus("Output failed to start");
                        tiktok.endLive(activeTikTokStreamId, [](bool, QString) {});
                        activeTikTokStreamId.clear();
                        setBusy(false);
                        QMessageBox::critical(this, "TikTok Output", outputError);
                        return;
                    }

                    setTikTokStatus("LIVE");
                    if (twitchEnabled->isChecked())
                        setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                    setBusy(false);
                    endLiveButton->setEnabled(true);
                });
        });
}

void T0GStreamDock::stopSelectedPlatforms()
{
    if (busy)
        return;

    setBusy(true);

    if (twitchEnabled->isChecked() && obs_frontend_streaming_active()) {
        setTwitchStatus("Stopping...");
        obs_frontend_streaming_stop();
    }

    tiktokOutput.stop();

    if (activeTikTokStreamId.isEmpty()) {
        setTikTokStatus(tiktok.hasToken() ? "Connected" : "Not connected");
        setBusy(false);
        endLiveButton->setEnabled(false);
        return;
    }

    const QString streamId = activeTikTokStreamId;
    activeTikTokStreamId.clear();
    setTikTokStatus("Ending LIVE...");

    tiktok.endLive(streamId, [this](bool ok, QString error) {
        setTikTokStatus(tiktok.hasToken() ? "Connected" : "Not connected");
        setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Ready when OBS is configured");
        setBusy(false);
        endLiveButton->setEnabled(false);

        if (!ok && !error.isEmpty())
            QMessageBox::warning(this, "TikTok", error);
    });
}

void T0GStreamDock::updateReadyState()
{
    const bool anyPlatform = twitchEnabled->isChecked() || tiktokEnabled->isChecked();
    updateButton->setEnabled(anyPlatform && !busy);
    goLiveButton->setEnabled(anyPlatform && !busy);
}
