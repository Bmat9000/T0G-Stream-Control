#include "t0g-stream-dock.hpp"
#include "credential-store.hpp"

#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

T0GStreamDock::T0GStreamDock(QWidget *parent) : QDockWidget("T0G Stream Control", parent)
{
    setObjectName("T0GStreamControlDock");
    setMinimumWidth(340);

    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);

    auto *header = new QHBoxLayout;
    auto *brand = new QLabel("T0G STREAM CONTROL", root);
    brand->setStyleSheet("font-size: 18px; font-weight: 700;");
    settingsButton = new QPushButton("SETTINGS", root);
    settingsButton->setMaximumWidth(100);
    header->addWidget(brand);
    header->addStretch();
    header->addWidget(settingsButton);
    layout->addLayout(header);

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
    auto *verticalStatus = new QLabel("Vertical: checking Aitum...", platforms);
    QTimer::singleShot(1000, verticalStatus, [this, verticalStatus] {
        verticalStatus->setText(aitumVertical.available()
            ? "Vertical: Aitum detected - TikTok will use the vertical canvas"
            : "Vertical: Aitum not detected - TikTok will use OBS fallback output");
    });

    loadTikTokButton = new QPushButton("LOAD TIKTOK FROM STREAMLABS", platforms);
    loadTikTokButton->setToolTip("Loads your existing Streamlabs TikTok session locally. The token is not saved by T0G.");

    platformLayout->addWidget(twitchEnabled);
    platformLayout->addWidget(twitchStatus);
    platformLayout->addSpacing(6);
    platformLayout->addWidget(tiktokEnabled);
    platformLayout->addWidget(tiktokStatus);
    platformLayout->addWidget(verticalStatus);
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

    connect(loadTikTokButton, &QPushButton::clicked, this, [this] { loadTikTokToken(false); });
    connect(settingsButton, &QPushButton::clicked, this, [this] { openSettings(); });
    connect(goLiveButton, &QPushButton::clicked, this, [this] { startSelectedPlatforms(); });
    connect(endLiveButton, &QPushButton::clicked, this, [this] { stopSelectedPlatforms(); });
    connect(updateButton, &QPushButton::clicked, this, [this] {
        QMessageBox::information(this, "T0G Stream Control",
                                 "Your title/game settings are ready. TikTok will apply them when you go live. Twitch metadata sync is not wired yet.");
    });
    connect(twitchEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });
    connect(tiktokEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });

    settings = T0GSettings::load();
    twitchEnabled->setChecked(settings.startTwitch);
    tiktokEnabled->setChecked(settings.startTikTok);
    loadSavedStreamInfo();

    updateReadyState();
    setWidget(root);

    if (settings.autoLoadTikTok)
        QTimer::singleShot(700, this, [this] { loadTikTokToken(true); });
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

void T0GStreamDock::loadTikTokToken(bool quiet)
{
    QString error;
    const QString token = TikTokService::loadTokenFromStreamlabsDesktop(&error);
    if (token.isEmpty()) {
        setTikTokStatus("Not connected");
        if (!quiet)
            QMessageBox::warning(this, "TikTok Connection", error);
        return;
    }

    tiktok.setToken(token);
    setTikTokStatus("Connected through Streamlabs");
    updateReadyState();
}

void T0GStreamDock::openSettings()
{
    T0GSettingsDialog dialog(settings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    settings = dialog.settings();
    settings.save();
    if (!settings.rememberStreamInfo)
        saveStreamInfo();
    updateReadyState();

    if (settings.autoLoadTikTok && !tiktok.hasToken())
        loadTikTokToken(true);
}

void T0GStreamDock::loadSavedStreamInfo()
{
    QSettings s("T0G", "T0G Stream Control");
    if (settings.rememberStreamInfo) {
        titleEdit->setText(s.value("last/title", settings.defaultTitle).toString());
        gameEdit->setText(s.value("last/game", settings.defaultGame).toString());
        audienceBox->setCurrentIndex(s.value("last/audience", settings.defaultAudience).toInt());
    } else {
        titleEdit->setText(settings.defaultTitle);
        gameEdit->setText(settings.defaultGame);
        audienceBox->setCurrentIndex(settings.defaultAudience);
    }
}

void T0GStreamDock::saveStreamInfo()
{
    QSettings s("T0G", "T0G Stream Control");
    if (settings.rememberStreamInfo) {
        s.setValue("last/title", titleEdit->text().trimmed());
        s.setValue("last/game", gameEdit->text().trimmed());
        s.setValue("last/audience", audienceBox->currentIndex());
    } else {
        s.remove("last");
    }
}

void T0GStreamDock::startSelectedPlatforms()
{
    if (busy)
        return;

    if (titleEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Missing title", "Enter a stream title first.");
        return;
    }

    saveStreamInfo();
    setBusy(true);

    if (twitchEnabled->isChecked() && settings.startTwitch) {
        if (settings.twitchConnectionMode == 1) {
            if (!startManualTwitch()) {
                setBusy(false);
                return;
            }
        } else if (!obs_frontend_streaming_active()) {
            setTwitchStatus("Starting...");
            obs_frontend_streaming_start();
        } else {
            setTwitchStatus("LIVE");
        }
    }

    if (tiktokEnabled->isChecked() && settings.startTikTok) {
        if (settings.tiktokConnectionMode == 1) {
            if (!startManualTikTok()) {
                setBusy(false);
                return;
            }
            setBusy(false);
            endLiveButton->setEnabled(true);
            return;
        }

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

bool T0GStreamDock::startManualTwitch()
{
    const QString key = CredentialStore::read("TwitchManualKey");
    QString error;
    if (settings.twitchManualServer.isEmpty() || key.isEmpty()) {
        QMessageBox::warning(this, "Manual Twitch RTMP",
                             "Open Settings > Twitch and enter the RTMP server and stream key.");
        return false;
    }
    if (!manualTwitchOutput.configure(settings.twitchManualServer, key, &error) ||
        !manualTwitchOutput.start(&error)) {
        QMessageBox::critical(this, "Manual Twitch RTMP", error);
        return false;
    }
    setTwitchStatus("LIVE - Manual RTMP");
    return true;
}

bool T0GStreamDock::startManualTikTok()
{
    const QString key = CredentialStore::read("TikTokManualKey");
    QString error;
    if (settings.tiktokManualServer.isEmpty() || key.isEmpty()) {
        QMessageBox::warning(this, "Manual TikTok RTMP",
                             "Open Settings > TikTok and enter the RTMP server and stream key.");
        return false;
    }

    usingAitumVertical = settings.preferVertical && aitumVertical.available();
    if (usingAitumVertical) {
        if (!aitumVertical.configureTikTok(settings.tiktokManualServer, key, &error) ||
            !aitumVertical.startTikTok(&error)) {
            usingAitumVertical = false;
            QMessageBox::critical(this, "Manual TikTok RTMP", error);
            return false;
        }
        setTikTokStatus("LIVE - Manual RTMP / Aitum Vertical");
        return true;
    }

    if (!manualTikTokOutput.configure(settings.tiktokManualServer, key, &error) ||
        !manualTikTokOutput.start(&error)) {
        QMessageBox::critical(this, "Manual TikTok RTMP", error);
        return false;
    }
    setTikTokStatus("LIVE - Manual RTMP");
    return true;
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
                    QString outputError;
                    usingAitumVertical = settings.preferVertical && aitumVertical.available();

                    if (usingAitumVertical) {
                        setTikTokStatus("Configuring Aitum Vertical...");
                        if (!aitumVertical.configureTikTok(result.server, result.key, &outputError) ||
                            !aitumVertical.startTikTok(&outputError)) {
                            setTikTokStatus("Aitum Vertical failed");
                            tiktok.endLive(activeTikTokStreamId, [](bool, QString) {});
                            activeTikTokStreamId.clear();
                            usingAitumVertical = false;
                            setBusy(false);
                            QMessageBox::critical(this, "TikTok Vertical Output", outputError);
                            return;
                        }

                        setTikTokStatus("LIVE - Aitum Vertical");
                        if (twitchEnabled->isChecked())
                            setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                        setBusy(false);
                        endLiveButton->setEnabled(true);
                        return;
                    }

                    setTikTokStatus("Configuring OBS fallback output...");
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

    if (settings.confirmBeforeEnd) {
        const auto answer = QMessageBox::question(
            this, "End selected streams",
            "End the selected live streams now?",
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return;
    }

    setBusy(true);

    if (twitchEnabled->isChecked() && settings.stopTwitch) {
        if (settings.twitchConnectionMode == 1) {
            manualTwitchOutput.stop();
            setTwitchStatus("Manual RTMP ready");
        } else if (obs_frontend_streaming_active()) {
            setTwitchStatus("Stopping...");
            obs_frontend_streaming_stop();
        }
    }

    if (tiktokEnabled->isChecked() && settings.stopTikTok) {
        if (usingAitumVertical) {
            QString ignored;
            aitumVertical.stopTikTok(&ignored);
            usingAitumVertical = false;
        } else if (settings.tiktokConnectionMode == 1) {
            manualTikTokOutput.stop();
        } else {
            tiktokOutput.stop();
        }
    }

    if (settings.tiktokConnectionMode == 1) {
        setTikTokStatus("Manual RTMP ready");
        setBusy(false);
        endLiveButton->setEnabled(false);
        return;
    }

    if (!settings.stopTikTok || !tiktokEnabled->isChecked() || activeTikTokStreamId.isEmpty()) {
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
