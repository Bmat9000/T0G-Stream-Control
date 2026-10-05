#include "t0g-stream-dock.hpp"
#include "credential-store.hpp"
#include "preflight-check.hpp"
#include "session-state.hpp"
#include "t0g-log.hpp"

#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QMenu>
#include <QSettings>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QApplication>
#include <QClipboard>

T0GStreamDock::T0GStreamDock(QWidget *parent) : QDockWidget("T0G Stream Control", parent)
{
    setObjectName("T0GStreamControlDock");
    setMinimumWidth(380);

    auto *root = new QWidget(this);
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *brand = new QLabel("T0G STREAM CONTROL", root);
    brand->setStyleSheet("font-size: 18px; font-weight: 800;");
    helpButton = new QPushButton("HELP", root);
    helpButton->setMaximumWidth(70);
    settingsButton = new QPushButton("SETTINGS", root);
    settingsButton->setMaximumWidth(100);
    header->addWidget(brand);
    header->addStretch();
    header->addWidget(helpButton);
    header->addWidget(settingsButton);
    layout->addLayout(header);

    auto *subtitle = new QLabel("Twitch + TikTok control from inside OBS", root);
    subtitle->setStyleSheet("color: palette(mid);");
    subtitle->setVisible(false);

    auto *streamInfo = new QGroupBox("STREAM DETAILS", root);
    auto *form = new QFormLayout(streamInfo);
    form->setContentsMargins(12, 14, 12, 12);
    form->setSpacing(8);
    titleEdit = new QLineEdit(root);
    titleEdit->setPlaceholderText("Ranked R6 | Road to Champ");
    gameEdit = new QLineEdit(root);
    gameEdit->setPlaceholderText("Rainbow Six Siege");
    audienceBox = new QComboBox(root);
    audienceBox->addItems({"Everyone", "Mature / 18+"});
    form->addRow("Title", titleEdit);
    form->addRow("Game", gameEdit);
    gameSuggestions = new QListWidget(root);
    gameSuggestions->setMaximumHeight(110);
    gameSuggestions->hide();
    form->addRow("", gameSuggestions);
    form->addRow("TikTok audience", audienceBox);
    layout->addWidget(streamInfo);

    auto *platforms = new QGroupBox("PLATFORMS", root);
    auto *platformLayout = new QVBoxLayout(platforms);
    platformLayout->setContentsMargins(12, 14, 12, 12);
    platformLayout->setSpacing(7);

    twitchEnabled = new QCheckBox("TWITCH", platforms);
    twitchEnabled->setChecked(true);
    twitchStatus = new QLabel("Twitch: Ready when OBS is configured", platforms);

    tiktokEnabled = new QCheckBox("TIKTOK", platforms);
    tiktokEnabled->setChecked(true);
    tiktokStatus = new QLabel("TikTok: Not connected", platforms);
    auto *verticalStatus = new QLabel("Vertical: checking Aitum...", platforms);
    QTimer::singleShot(1000, verticalStatus, [this, verticalStatus] {
        verticalStatus->setText(aitumVertical.available()
            ? "Vertical: Aitum detected - TikTok will use the vertical canvas"
            : "Vertical: Aitum not detected - TikTok will use OBS fallback output");
    });

    auto *tiktokAccount = new QGroupBox("TikTok Account", platforms);
    auto *tiktokAccountLayout = new QFormLayout(tiktokAccount);
    tiktokUsername = new QLabel("Unknown", tiktokAccount);
    tiktokApproval = new QLabel("Unknown", tiktokAccount);
    tiktokCanLive = new QLabel("Unknown", tiktokAccount);
    tiktokAccountLayout->addRow("Username", tiktokUsername);
    tiktokAccountLayout->addRow("Status", tiktokApproval);
    tiktokAccountLayout->addRow("Can Go Live", tiktokCanLive);

    auto *loadButtons = new QHBoxLayout;
    loadTikTokWebButton = new QPushButton("CONNECT TIKTOK", platforms);
    loadTikTokWebButton->setToolTip("Open Streamlabs login in your browser and connect TikTok to T0G.");
    loadButtons->addWidget(loadTikTokWebButton);
    refreshTikTokButton = new QPushButton("REFRESH", platforms);
    refreshTikTokButton->setMaximumWidth(95);

    platformLayout->addWidget(twitchEnabled);
    platformLayout->addWidget(twitchStatus);
    platformLayout->addSpacing(6);
    platformLayout->addWidget(tiktokEnabled);
    platformLayout->addWidget(tiktokStatus);
    verticalStatus->setStyleSheet("color: palette(mid); font-size: 11px;");
    platformLayout->addWidget(verticalStatus);
    platformLayout->addWidget(tiktokAccount);
    loadButtons->addWidget(refreshTikTokButton);
    platformLayout->addLayout(loadButtons);
    layout->addWidget(platforms);

    updateButton = new QPushButton("UPDATE TITLE / GAME", root);
    updateButton->setToolTip("TikTok uses these fields when T0G creates the LIVE session. Twitch metadata API wiring is the next milestone.");

    auto *buttons = new QHBoxLayout;
    goLiveButton = new QPushButton("GO LIVE", root);
    goLiveButton->setMinimumHeight(46);
    goLiveButton->setStyleSheet("font-weight: 700;");
    endLiveButton = new QPushButton("END LIVE", root);
    endLiveButton->setMinimumHeight(46);
    endLiveButton->setEnabled(false);
    buttons->addWidget(goLiveButton);
    buttons->addWidget(endLiveButton);

    auto *actions = new QGroupBox("STREAM CONTROL", root);
    auto *actionsLayout = new QVBoxLayout(actions);
    actionsLayout->setContentsMargins(12, 14, 12, 12);
    actionsLayout->setSpacing(8);
    actionsLayout->addWidget(updateButton);
    actionsLayout->addLayout(buttons);
    layout->addWidget(actions);
    layout->addStretch();

    connect(loadTikTokWebButton, &QPushButton::clicked, this, [this] { loadTikTokFromWeb(); });
    connect(refreshTikTokButton, &QPushButton::clicked, this, [this] { refreshTikTokAccount(); });
    gameSearchTimer = new QTimer(this);
    gameSearchTimer->setSingleShot(true);
    gameSearchTimer->setInterval(300);
    connect(gameSearchTimer, &QTimer::timeout, this, [this] { runGameSearch(); });
    connect(gameEdit, &QLineEdit::textEdited, this, [this] {
        selectedTikTokCategoryId.clear();
        scheduleGameSearch();
    });
    connect(gameSuggestions, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        gameEdit->setText(item->text());
        selectedTikTokCategoryId = item->data(Qt::UserRole).toString();
        gameSuggestions->hide();
    });
    connect(helpButton, &QPushButton::clicked, this, [this] { openHelpMenu(); });
    connect(settingsButton, &QPushButton::clicked, this, [this] { openSettings(); });
    connect(goLiveButton, &QPushButton::clicked, this, [this] { startSelectedPlatforms(); });
    connect(endLiveButton, &QPushButton::clicked, this, [this] { stopSelectedPlatforms(); });
    connect(updateButton, &QPushButton::clicked, this, [this] {
        if (!twitchEnabled->isChecked() || settings.twitchConnectionMode == 1) {
            QMessageBox::information(this, "T0G Stream Control",
                                     "TikTok will apply the title/game when its LIVE session is created.");
            return;
        }
        twitch.updateChannel(titleEdit->text().trimmed(), gameEdit->text().trimmed(),
            [this](bool ok, QString msg) {
                if (ok) QMessageBox::information(this, "Twitch Updated", msg);
                else QMessageBox::warning(this, "Twitch Update Failed", msg);
            });
    });
    connect(twitchEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });
    connect(tiktokEnabled, &QCheckBox::toggled, this, [this] { updateReadyState(); });

    settings = T0GSettings::load();
    twitch.setClientId(settings.twitchClientId);
    twitch.restore([this](bool ok, QString name) {
        setTwitchStatus(ok ? ("Connected as " + name) : "Ready when OBS is configured");
    });
    connect(&twitch, &TwitchService::accountChanged, this, [this] {
        const QString name = twitch.displayName();
        setTwitchStatus(name.isEmpty() ? "Ready when OBS is configured" : ("Connected as " + name));
    });
    twitchEnabled->setChecked(settings.startTwitch);
    tiktokEnabled->setChecked(settings.startTikTok);
    loadSavedStreamInfo();

    T0GLog::write("Dock initialized; Aitum Vertical detected=" + QString(aitumVertical.available() ? "true" : "false"));
    updateReadyState();
    setWidget(root);

}

void T0GStreamDock::setBusy(bool value)
{
    busy = value;
    loadTikTokWebButton->setEnabled(!busy);
    updateReadyState();
}

void T0GStreamDock::setTikTokStatus(const QString &text)
{
    tiktokStatus->setText("TikTok: " + text);
    T0GLog::write("TikTok status: " + text);
}

void T0GStreamDock::setTwitchStatus(const QString &text)
{
    twitchStatus->setText("Twitch: " + text);
    T0GLog::write("Twitch status: " + text);
}

void T0GStreamDock::loadTikTokFromWeb()
{
    setBusy(true);
    setTikTokStatus("Waiting for Streamlabs web login...");
    tiktok.loadTokenFromWeb([this](bool ok, QString, QString error) {
        setBusy(false);
        if (!ok) {
            setTikTokStatus("Web login failed");
            QMessageBox::warning(this, "TikTok Web Login", error);
            return;
        }
        setTikTokStatus("Connected through Streamlabs");
        refreshTikTokAccount();
        updateReadyState();
    });
}

void T0GStreamDock::refreshTikTokAccount()
{
    if (!tiktok.hasToken()) {
        tiktokUsername->setText("Unknown");
        tiktokApproval->setText("Unknown");
        tiktokCanLive->setText("False");
        return;
    }

    tiktok.getAccountInfo([this](TikTokAccountInfo info) {
        if (!info.ok) {
            setTikTokStatus("Account info failed");
            return;
        }
        tiktokUsername->setText(info.username.isEmpty() ? "Unknown" : info.username);
        tiktokApproval->setText(info.status.isEmpty() ? "Unknown" : info.status);
        tiktokCanLive->setText(info.canGoLive ? "True" : "False");
        setTikTokStatus(info.canGoLive ? "Connected through Streamlabs" : "Connected - LIVE unavailable");
        updateReadyState();
    });
}

void T0GStreamDock::scheduleGameSearch()
{
    if (!tiktok.hasToken() || gameEdit->text().trimmed().isEmpty()) {
        gameSuggestions->hide();
        return;
    }
    gameSearchTimer->start();
}

void T0GStreamDock::runGameSearch()
{
    const QString query = gameEdit->text().trimmed();
    if (query.isEmpty() || !tiktok.hasToken()) {
        gameSuggestions->hide();
        return;
    }

    tiktok.searchCategories(query, [this, query](bool ok, QVector<TikTokCategory> categories, QString) {
        if (gameEdit->text().trimmed() != query)
            return;
        gameSuggestions->clear();
        if (!ok) {
            gameSuggestions->hide();
            return;
        }
        for (const auto &category : categories) {
            auto *item = new QListWidgetItem(category.name, gameSuggestions);
            item->setData(Qt::UserRole, category.id);
        }
        gameSuggestions->setVisible(gameSuggestions->count() > 0);
    });
}

void T0GStreamDock::openHelpMenu()
{
    QMenu menu(this);
    QAction *aitumSetup = menu.addAction("Aitum TikTok Setup Guide");
    QAction *viewLogs = menu.addAction("View Logs");
    QAction *chosen = menu.exec(helpButton->mapToGlobal(QPoint(0, helpButton->height())));
    if (chosen == aitumSetup)
        showAitumTikTokSetupGuide();
    else if (chosen == viewLogs)
        showPluginLogs();
}


void T0GStreamDock::showAitumTikTokSetupGuide()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Aitum TikTok Setup Guide");
    dialog.resize(650, 430);
    auto *layout = new QVBoxLayout(&dialog);

    auto *intro = new QLabel(
        "<b>Create this output in Aitum Vertical once.</b><br><br>"
        "In OBS, open <b>Aitum Vertical</b> &gt; <b>Vertical Settings</b> &gt; <b>Streaming</b>. "
        "Add/enable an RTMP output and enter the values below. The output name must be exact. "
        "After it exists, T0G can update the server/key automatically each time you go live.",
        &dialog);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto addCopyRow = [&](const QString &labelText, const QString &value, bool secret) {
        auto *label = new QLabel(labelText, &dialog);
        label->setStyleSheet("font-weight: 600;");
        layout->addWidget(label);
        auto *row = new QHBoxLayout;
        auto *edit = new QLineEdit(value, &dialog);
        edit->setReadOnly(true);
        if (secret)
            edit->setEchoMode(QLineEdit::Password);
        auto *copy = new QPushButton("COPY", &dialog);
        copy->setMaximumWidth(80);
        connect(copy, &QPushButton::clicked, &dialog, [edit, copy] {
            QApplication::clipboard()->setText(edit->text());
            copy->setText("COPIED");
            QTimer::singleShot(1200, copy, [copy] { copy->setText("COPY"); });
        });
        row->addWidget(edit);
        row->addWidget(copy);
        layout->addLayout(row);
    };

    addCopyRow("1. Output Name — paste into Aitum's Name field", "T0G TikTok", false);

    const QString server = SessionState::tikTokServer();
    const QString key = SessionState::tikTokKey();
    addCopyRow("2. RTMP Server — paste into Aitum's Server / URL field",
               server.isEmpty() ? "Create a TikTok LIVE in T0G first to get the current server" : server, false);
    addCopyRow("3. Stream Key — paste into Aitum's Stream Key field",
               key.isEmpty() ? "Create a TikTok LIVE in T0G first to get the current stream key" : key, true);

    auto *note = new QLabel(
        "<b>Then:</b> make sure the output is enabled, save Aitum Vertical Settings, close the window, "
        "and press GO LIVE in T0G again.<br><br>"
        "<b>Important:</b> the name must be exactly <code>T0G TikTok</code>. "
        "Do not share your stream key with anyone.",
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void T0GStreamDock::showPluginLogs()
{
    QDialog dialog(this);
    dialog.setWindowTitle("T0G Stream Control Logs");
    dialog.resize(900, 600);
    auto *layout = new QVBoxLayout(&dialog);
    auto *viewer = new QPlainTextEdit(&dialog);
    viewer->setReadOnly(true);
    viewer->setLineWrapMode(QPlainTextEdit::NoWrap);
    viewer->setPlainText(T0GLog::text());
    viewer->moveCursor(QTextCursor::End);
    layout->addWidget(viewer);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    auto *refresh = buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    auto *copy = buttons->addButton("Copy All", QDialogButtonBox::ActionRole);
    connect(refresh, &QPushButton::clicked, &dialog, [viewer] { viewer->setPlainText(T0GLog::text()); viewer->moveCursor(QTextCursor::End); });
    connect(copy, &QPushButton::clicked, &dialog, [viewer] { viewer->selectAll(); viewer->copy(); viewer->moveCursor(QTextCursor::End); });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}

void T0GStreamDock::openSettings()
{
    T0GSettingsDialog dialog(settings, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    settings = dialog.settings();
    settings.save();
    twitch.setClientId(settings.twitchClientId);
    twitch.restore([this](bool ok, QString name) {
        setTwitchStatus(ok ? ("Connected as " + name) : "Ready when OBS is configured");
    });
    if (!settings.rememberStreamInfo)
        saveStreamInfo();
    updateReadyState();

}

void T0GStreamDock::showTikTokFallbackFailure(const QString &detail)
{
    QMessageBox box(QMessageBox::Critical,
                    "TikTok Output Setup Failed",
                    "T0G created your TikTok LIVE and retrieved the stream credentials, "
                    "but automatic output setup failed.\n\n"
                    "Your RTMP server and stream key are still available.\n\n"
                    "Go to Settings > Current Stream Credentials / Fallback to copy them into "
                    "OBS, Aitum, Streamlabs, or another streaming program.\n\n"
                    "Error: " + detail + "\n\n"
                    "Need help setting up Aitum? Open T0G > HELP > Aitum TikTok Setup Guide. "
                    "It shows exactly what goes in each field and gives you COPY buttons.",
                    QMessageBox::NoButton, this);
    auto *guide = box.addButton("Open Aitum Setup Guide", QMessageBox::AcceptRole);
    auto *open = box.addButton("Open Settings", QMessageBox::ActionRole);
    box.addButton("Close", QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == guide)
        showAitumTikTokSetupGuide();
    else if (box.clickedButton() == open)
        openSettings();
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
    T0GLog::write(QString("GO LIVE requested; Twitch=%1 TikTok=%2 TikTokPath=%3").arg(twitchEnabled->isChecked() ? "on" : "off", tiktokEnabled->isChecked() ? "on" : "off", settings.tiktokOutputTestMode == 1 ? "Direct OBS RTMP Test" : "Aitum Vertical"));
    if (busy)
        return;

    const auto preflight = PreflightCheck::run(settings, twitchEnabled->isChecked(), tiktokEnabled->isChecked(),
                                               titleEdit->text(), tiktok.hasToken(), aitumVertical.available());
    if (!preflight.ok) {
        QMessageBox::warning(this, "Stream Pre-flight Failed",
                             "T0G found the following before starting anything:\n\n" + preflight.message());
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
                                 "Click LOAD FROM WEB first.");
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

    usingAitumVertical = settings.tiktokOutputTestMode == 0 && settings.preferVertical && aitumVertical.available();
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

    if (!selectedTikTokCategoryId.isNull()) {
        const QString categoryId = selectedTikTokCategoryId;
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
                SessionState::setTikTokCredentials(result.server, result.key, result.streamId);
                QString outputError;
                usingAitumVertical = settings.tiktokOutputTestMode == 0 && settings.preferVertical && aitumVertical.available();
                if (usingAitumVertical) {
                    setTikTokStatus("Configuring Aitum Vertical...");
                    if (!aitumVertical.configureTikTok(result.server, result.key, &outputError) ||
                        !aitumVertical.startTikTok(&outputError)) {
                        setTikTokStatus("Output setup failed - credentials available in Settings");
                        usingAitumVertical = false; setBusy(false); endLiveButton->setEnabled(true);
                        showTikTokFallbackFailure(outputError); return;
                    }
                    setTikTokStatus("LIVE - Aitum Vertical");
                    if (twitchEnabled->isChecked()) setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                    setBusy(false); endLiveButton->setEnabled(true); return;
                }
                setTikTokStatus(settings.tiktokOutputTestMode == 1 ? "Starting Direct OBS RTMP Test..." : "Configuring OBS fallback output...");
                if (!tiktokOutput.configure(result.server, result.key, &outputError) || !tiktokOutput.start(&outputError)) {
                    setTikTokStatus("Output setup failed - credentials available in Settings");
                    setBusy(false); endLiveButton->setEnabled(true); showTikTokFallbackFailure(outputError); return;
                }
                setTikTokStatus(settings.tiktokOutputTestMode == 1 ? "LIVE - Direct OBS RTMP Test" : "LIVE");
                if (twitchEnabled->isChecked()) setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                setBusy(false); endLiveButton->setEnabled(true);
            });
        return;
    }

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

                    // Session-only fallback credentials. Never logged.
                    SessionState::setTikTokCredentials(result.server, result.key, result.streamId);

                    QString outputError;
                    usingAitumVertical = settings.tiktokOutputTestMode == 0 && settings.preferVertical && aitumVertical.available();

                    if (usingAitumVertical) {
                        setTikTokStatus("Configuring Aitum Vertical...");
                        if (!aitumVertical.configureTikTok(result.server, result.key, &outputError) ||
                            !aitumVertical.startTikTok(&outputError)) {
                            setTikTokStatus("Output setup failed - credentials available in Settings");
                            usingAitumVertical = false;
                            setBusy(false);
                            endLiveButton->setEnabled(true);
                            showTikTokFallbackFailure(outputError);
                            return;
                        }

                        setTikTokStatus("LIVE - Aitum Vertical");
                        if (twitchEnabled->isChecked())
                            setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                        setBusy(false);
                        endLiveButton->setEnabled(true);
                        return;
                    }

                    setTikTokStatus(settings.tiktokOutputTestMode == 1 ? "Starting Direct OBS RTMP Test..." : "Configuring OBS fallback output...");
                    if (!tiktokOutput.configure(result.server, result.key, &outputError)) {
                        setTikTokStatus("Output setup failed - credentials available in Settings");
                        setBusy(false);
                        endLiveButton->setEnabled(true);
                        showTikTokFallbackFailure(outputError);
                        return;
                    }

                    if (!tiktokOutput.start(&outputError)) {
                        setTikTokStatus("Output failed to start - credentials available in Settings");
                        setBusy(false);
                        endLiveButton->setEnabled(true);
                        showTikTokFallbackFailure(outputError);
                        return;
                    }

                    setTikTokStatus(settings.tiktokOutputTestMode == 1 ? "LIVE - Direct OBS RTMP Test" : "LIVE");
                    if (twitchEnabled->isChecked())
                        setTwitchStatus(obs_frontend_streaming_active() ? "LIVE" : "Starting...");
                    setBusy(false);
                    endLiveButton->setEnabled(true);
                });
        });
}

void T0GStreamDock::stopSelectedPlatforms()
{
    T0GLog::write("END LIVE requested");
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
    SessionState::clearTikTok();
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
