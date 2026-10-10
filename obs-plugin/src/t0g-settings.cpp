#include "t0g-settings.hpp"
#include "replay-controller.hpp"
#include <QKeySequenceEdit>
#include <QTimer>
#include "credential-store.hpp"
#include "twitch-service.hpp"

#include <QCheckBox>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {
QSettings store() { return QSettings("T0G", "T0G Stream Control"); }
constexpr const char *kT0GTwitchClientId = "91gdjfnblk3jb7e2wglcu6hvrv0mi2";
}

T0GSettings T0GSettings::load()
{
    QSettings s = store();
    T0GSettings out;
    out.autoReplay = s.value("replay/auto", true).toBool();
    out.clipDuration = s.value("replay/duration", 60).toInt();
    if (!QList<int>{15,30,60,120,300}.contains(out.clipDuration)) out.clipDuration = 60;
    out.autoLoadTikTok = s.value("tiktok/autoLoad", true).toBool();
    out.rememberStreamInfo = s.value("stream/rememberInfo", true).toBool();
    out.confirmBeforeEnd = s.value("stream/confirmEnd", true).toBool();
    out.startTwitch = s.value("platforms/startTwitch", true).toBool();
    out.startTikTok = s.value("platforms/startTikTok", true).toBool();
    out.stopTwitch = s.value("platforms/stopTwitch", true).toBool();
    out.stopTikTok = s.value("platforms/stopTikTok", true).toBool();
    out.preferVertical = s.value("video/preferVertical", true).toBool();
    out.tiktokOutputTestMode = s.value("video/tiktokOutputTestMode", 0).toInt();
    out.defaultTitle = s.value("defaults/title").toString();
    out.defaultGame = s.value("defaults/game", "Rainbow Six Siege").toString();
    out.defaultAudience = s.value("defaults/audience", 0).toInt();
    out.twitchConnectionMode = s.value("accounts/twitchMode", 0).toInt();
    out.twitchManualServer = s.value("accounts/twitchServer", "rtmp://live.twitch.tv/app").toString();
    out.twitchClientId = QString::fromUtf8(kT0GTwitchClientId);
    out.tiktokConnectionMode = s.value("accounts/tiktokMode", 0).toInt();
    out.tiktokManualServer = s.value("accounts/tiktokServer").toString();
    return out;
}

void T0GSettings::save() const
{
    QSettings s = store();
    s.setValue("replay/auto", autoReplay);
    s.setValue("replay/duration", clipDuration);
    s.setValue("tiktok/autoLoad", autoLoadTikTok);
    s.setValue("stream/rememberInfo", rememberStreamInfo);
    s.setValue("stream/confirmEnd", confirmBeforeEnd);
    s.setValue("platforms/startTwitch", startTwitch);
    s.setValue("platforms/startTikTok", startTikTok);
    s.setValue("platforms/stopTwitch", stopTwitch);
    s.setValue("platforms/stopTikTok", stopTikTok);
    s.setValue("video/preferVertical", preferVertical);
    s.setValue("video/tiktokOutputTestMode", tiktokOutputTestMode);
    s.setValue("defaults/title", defaultTitle);
    s.setValue("defaults/game", defaultGame);
    s.setValue("defaults/audience", defaultAudience);
    s.setValue("accounts/twitchMode", twitchConnectionMode);
    s.setValue("accounts/twitchServer", twitchManualServer);
    s.remove("accounts/twitchClientId");
    s.setValue("accounts/tiktokMode", tiktokConnectionMode);
    s.setValue("accounts/tiktokServer", tiktokManualServer);
    s.sync();
}

T0GSettingsDialog::T0GSettingsDialog(const T0GSettings &cfg, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("T0G Stream Control Settings");
    setMinimumWidth(520);
    resize(620, 700);

    auto *dialogLayout = new QVBoxLayout(this);
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *content = new QWidget(scrollArea);
    auto *layout = new QVBoxLayout(content);

    auto *accounts = new QGroupBox("Accounts / Connection", content);
    auto *accountsLayout = new QVBoxLayout(accounts);

    auto *tw = new QGroupBox("Twitch", accounts);
    auto *twForm = new QFormLayout(tw);
    twitchMode = new QComboBox(tw);
    twitchMode->addItems({"OBS / Account Login", "Manual RTMP"});
    twitchServer = new QLineEdit(tw);
    twitchKey = new QLineEdit(tw);
    twitchKey->setEchoMode(QLineEdit::Password);
    twitchKey->setPlaceholderText("Stored securely in Windows Credential Manager");
    clearTwitch = new QPushButton("Clear saved key", tw);
    twitchAccountStatus = new QLabel("Not connected", tw);
    connectTwitch = new QPushButton("Connect Twitch", tw);
    disconnectTwitch = new QPushButton("Disconnect", tw);
    auto *twAccountButtons = new QWidget(tw);
    auto *twAccountButtonsLayout = new QHBoxLayout(twAccountButtons);
    twAccountButtonsLayout->setContentsMargins(0,0,0,0);
    twAccountButtonsLayout->addWidget(connectTwitch);
    twAccountButtonsLayout->addWidget(disconnectTwitch);
    twForm->addRow("Mode", twitchMode);
    twForm->addRow("Account", twitchAccountStatus);
    twForm->addRow("", twAccountButtons);
    twForm->addRow("RTMP server", twitchServer);
    twForm->addRow("Stream key", twitchKey);
    twForm->addRow("", clearTwitch);
    accountsLayout->addWidget(tw);

    auto *tt = new QGroupBox("TikTok", accounts);
    auto *ttForm = new QFormLayout(tt);
    tiktokMode = new QComboBox(tt);
    tiktokMode->addItems({"Automatic / Streamlabs", "Manual RTMP"});
    tiktokServer = new QLineEdit(tt);
    tiktokKey = new QLineEdit(tt);
    tiktokKey->setEchoMode(QLineEdit::Password);
    tiktokKey->setPlaceholderText("Stored securely in Windows Credential Manager");
    clearTikTok = new QPushButton("Clear saved key", tt);
    ttForm->addRow("Mode", tiktokMode);
    ttForm->addRow("RTMP server", tiktokServer);
    ttForm->addRow("Stream key", tiktokKey);
    ttForm->addRow("", clearTikTok);
    accountsLayout->addWidget(tt);
    layout->addWidget(accounts);


    auto *credentials = new QGroupBox("Current Stream Credentials / Fallback", content);
    auto *credentialsForm = new QFormLayout(credentials);

    liveTikTokServer = new QLineEdit(credentials);
    liveTikTokServer->setReadOnly(true);
    liveTikTokServer->setPlaceholderText("Available after TikTok LIVE is created");
    auto *serverRow = new QWidget(credentials);
    auto *serverLayout = new QHBoxLayout(serverRow);
    serverLayout->setContentsMargins(0, 0, 0, 0);
    auto *copyServer = new QPushButton("Copy", serverRow);
    serverLayout->addWidget(liveTikTokServer);
    serverLayout->addWidget(copyServer);

    liveTikTokKey = new QLineEdit(credentials);
    liveTikTokKey->setReadOnly(true);
    liveTikTokKey->setEchoMode(QLineEdit::Password);
    liveTikTokKey->setPlaceholderText("Available after TikTok LIVE is created");
    auto *keyRow = new QWidget(credentials);
    auto *keyLayout = new QHBoxLayout(keyRow);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    showLiveTikTokKey = new QPushButton("Show", keyRow);
    auto *copyKey = new QPushButton("Copy", keyRow);
    keyLayout->addWidget(liveTikTokKey);
    keyLayout->addWidget(showLiveTikTokKey);
    keyLayout->addWidget(copyKey);

    auto *note = new QLabel(
        "These are the current TikTok LIVE credentials. Use them if automatic OBS/Aitum setup fails. "
        "Automatic credentials are session-only and are cleared when the LIVE ends.", credentials);
    note->setWordWrap(true);

    credentialsForm->addRow("TikTok RTMP server", serverRow);
    credentialsForm->addRow("TikTok stream key", keyRow);
    credentialsForm->addRow("", note);
    layout->addWidget(credentials);

    connect(copyServer, &QPushButton::clicked, this, [this] { copyField(liveTikTokServer); });
    connect(copyKey, &QPushButton::clicked, this, [this] { copyField(liveTikTokKey); });
    connect(showLiveTikTokKey, &QPushButton::clicked, this,
            [this] { toggleSecret(liveTikTokKey, showLiveTikTokKey); });
    refreshLiveCredentials();

    auto *replay = new QGroupBox("Replay Buffers & Clips", content);
    auto *replayForm = new QFormLayout(replay);
    autoReplay = new QCheckBox("Auto start/stop replay buffers when streams start/end", replay);
    autoReplay->setChecked(cfg.autoReplay);
    replayForm->addRow(autoReplay);
    clipDuration = new QComboBox(replay);
    for (int seconds : {15,30,60,120,300}) clipDuration->addItem(QString("%1 seconds").arg(seconds), seconds);
    clipDuration->setCurrentIndex(clipDuration->findData(cfg.clipDuration));
    replayForm->addRow("Clip duration (both layouts)", clipDuration);
    QSettings replaySettings("T0G", "T0G Stream Control");
    const QStringList labels = {"Start Replay Buffers", "Stop Replay Buffers", "Save Both Clips"};
    for (int i = 0; i < 3; ++i) {
        clipKeys[i] = new QKeySequenceEdit(QKeySequence(replaySettings.value(QString("replay/shortcut%1").arg(i)).toString()), replay);
        clipKeys[i]->setMaximumSequenceLength(1);
        replayForm->addRow(labels[i], clipKeys[i]);
    }
    auto *replayStatus = new QLabel(replay);
    auto *replayTimer = new QTimer(replay);
    connect(replayTimer, &QTimer::timeout, replay, [replayStatus] {
        if (auto *controller = ReplayController::instance()) replayStatus->setText(controller->status());
    });
    replayTimer->start(500);
    replayForm->addRow("Status", replayStatus);
    auto *replayNote = new QLabel("Click a shortcut and press your key combination. You can also assign these actions in OBS Settings > Hotkeys. Clips use OBS and Aitum's existing save folders. Duration changes apply to new footage; a newly started buffer may have less footage available.", replay);
    replayNote->setWordWrap(true);
    replayForm->addRow(replayNote);
    layout->addWidget(replay);

    auto *general = new QGroupBox("General", content);
    auto *generalLayout = new QVBoxLayout(general);
    autoLoadTikTok = new QCheckBox("Automatically restore saved Streamlabs API token when OBS starts", general);
    rememberStreamInfo = new QCheckBox("Remember last stream title, game and audience", general);
    confirmBeforeEnd = new QCheckBox("Confirm before ending all selected streams", general);
    generalLayout->addWidget(autoLoadTikTok);
    generalLayout->addWidget(rememberStreamInfo);
    generalLayout->addWidget(confirmBeforeEnd);
    layout->addWidget(general);

    auto *platforms = new QGroupBox("GO LIVE / END LIVE behavior", content);
    auto *platformLayout = new QVBoxLayout(platforms);
    startTwitch = new QCheckBox("GO LIVE starts Twitch when Twitch is selected", platforms);
    startTikTok = new QCheckBox("GO LIVE starts TikTok when TikTok is selected", platforms);
    stopTwitch = new QCheckBox("END LIVE stops Twitch when Twitch is selected", platforms);
    stopTikTok = new QCheckBox("END LIVE stops TikTok when TikTok is selected", platforms);
    platformLayout->addWidget(startTwitch); platformLayout->addWidget(startTikTok);
    platformLayout->addWidget(stopTwitch); platformLayout->addWidget(stopTikTok);
    layout->addWidget(platforms);

    auto *video = new QGroupBox("TikTok video", content);
    auto *videoLayout = new QVBoxLayout(video);
    preferVertical = new QCheckBox("Prefer Aitum Vertical output when available", video);
    videoLayout->addWidget(preferVertical);
    auto *outputTestLabel = new QLabel("TikTok output path (diagnostic)", video);
    tiktokOutputTestMode = new QComboBox(video);
    tiktokOutputTestMode->addItems({"Aitum Vertical (normal)", "Direct OBS RTMP Test"});
    auto *outputTestNote = new QLabel("Direct OBS RTMP Test bypasses Aitum but uses the same Streamlabs LIVE session, RTMP server and key. Use this only while diagnosing TikTok disconnects.", video);
    outputTestNote->setWordWrap(true);
    videoLayout->addWidget(outputTestLabel);
    videoLayout->addWidget(tiktokOutputTestMode);
    videoLayout->addWidget(outputTestNote);
    layout->addWidget(video);

    auto *defaults = new QGroupBox("Defaults", content);
    auto *form = new QFormLayout(defaults);
    defaultTitle = new QLineEdit(defaults); defaultGame = new QLineEdit(defaults);
    defaultAudience = new QComboBox(defaults); defaultAudience->addItems({"Everyone", "Mature / 18+"});
    form->addRow("Default title", defaultTitle); form->addRow("Default game", defaultGame);
    form->addRow("TikTok audience", defaultAudience);
    layout->addWidget(defaults);

    twitchMode->setCurrentIndex(cfg.twitchConnectionMode);
    twitchServer->setText(cfg.twitchManualServer);
    twitchKey->setText(CredentialStore::read("TwitchManualKey"));
    tiktokMode->setCurrentIndex(cfg.tiktokConnectionMode);
    tiktokServer->setText(cfg.tiktokManualServer);
    tiktokKey->setText(CredentialStore::read("TikTokManualKey"));
    autoLoadTikTok->setChecked(cfg.autoLoadTikTok);
    rememberStreamInfo->setChecked(cfg.rememberStreamInfo);
    confirmBeforeEnd->setChecked(cfg.confirmBeforeEnd);
    startTwitch->setChecked(cfg.startTwitch); startTikTok->setChecked(cfg.startTikTok);
    stopTwitch->setChecked(cfg.stopTwitch); stopTikTok->setChecked(cfg.stopTikTok);
    preferVertical->setChecked(cfg.preferVertical);
    tiktokOutputTestMode->setCurrentIndex(cfg.tiktokOutputTestMode);
    defaultTitle->setText(cfg.defaultTitle); defaultGame->setText(cfg.defaultGame);
    defaultAudience->setCurrentIndex(cfg.defaultAudience);

    twitchService = new TwitchService(this);
    twitchService->setClientId(cfg.twitchClientId);
    connect(twitchService, &TwitchService::activationRequired, this, [this](const QString &url, const QString &code) {
        QMessageBox::information(this, "Connect Twitch",
            "Your browser has been opened.\n\nEnter this Twitch activation code if requested:\n\n" + code +
            "\n\nActivation page: " + url);
    });
    twitchService->restore([this](bool ok, QString name) {
        twitchAccountStatus->setText(ok ? ("Connected as " + name) : "Not connected");
    });
    connect(twitchService, &TwitchService::accountChanged, this, [this] {
        const QString name = twitchService->displayName();
        twitchAccountStatus->setText(name.isEmpty() ? "Not connected" : ("Connected as " + name));
    });
    connect(connectTwitch, &QPushButton::clicked, this, [this] {
        twitchService->setClientId(QString::fromUtf8(kT0GTwitchClientId));
        twitchService->connectDevice([this](bool ok, QString msg) {
            twitchAccountStatus->setText(ok ? ("Connected as " + msg) : "Not connected");
            if (!ok) QMessageBox::warning(this, "Twitch Login", msg);
        });
    });
    connect(disconnectTwitch, &QPushButton::clicked, this, [this] {
        twitchService->disconnectAccount();
        twitchAccountStatus->setText("Not connected");
    });
    connect(twitchMode, &QComboBox::currentIndexChanged, this, [this] { updateAccountFields(); });
    connect(tiktokMode, &QComboBox::currentIndexChanged, this, [this] { updateAccountFields(); });
    connect(clearTwitch, &QPushButton::clicked, this, [this] { clearTwitchKey(); });
    connect(clearTikTok, &QPushButton::clicked, this, [this] { clearTikTokKey(); });
    updateAccountFields();

    layout->addStretch();
    scrollArea->setWidget(content);
    dialogLayout->addWidget(scrollArea);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveSecrets();
        if (auto *controller = ReplayController::instance())
            for (int i = 0; i < 3; ++i)
                if (clipKeys[i]->keySequence().toString() != QSettings("T0G", "T0G Stream Control").value(QString("replay/shortcut%1").arg(i)).toString())
                    controller->applyHotkey(i, clipKeys[i]->keySequence().toString());
        accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    dialogLayout->addWidget(buttons);
}

void T0GSettingsDialog::refreshLiveCredentials()
{
    QSettings s("T0G", "T0G Stream Control Session");
    liveTikTokServer->setText(s.value("tiktok/server").toString());
    liveTikTokKey->setText(s.value("tiktok/key").toString());
}

void T0GSettingsDialog::copyField(QLineEdit *field)
{
    if (!field || field->text().isEmpty())
        return;
    QApplication::clipboard()->setText(field->text());
}

void T0GSettingsDialog::toggleSecret(QLineEdit *field, QPushButton *button)
{
    if (!field || !button)
        return;
    const bool hidden = field->echoMode() == QLineEdit::Password;
    field->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    button->setText(hidden ? "Hide" : "Show");
}

void T0GSettingsDialog::updateAccountFields()
{
    const bool twManual = twitchMode->currentIndex() == 1;
    twitchServer->setEnabled(twManual); twitchKey->setEnabled(twManual); clearTwitch->setEnabled(twManual);
    const bool ttManual = tiktokMode->currentIndex() == 1;
    tiktokServer->setEnabled(ttManual); tiktokKey->setEnabled(ttManual); clearTikTok->setEnabled(ttManual);
    autoLoadTikTok->setEnabled(!ttManual);
}

void T0GSettingsDialog::saveSecrets()
{
    QString error;
    if (twitchMode->currentIndex() == 1 && !twitchKey->text().isEmpty() &&
        !CredentialStore::write("TwitchManualKey", twitchKey->text(), &error))
        QMessageBox::warning(this, "Twitch stream key", error);
    error.clear();
    if (tiktokMode->currentIndex() == 1 && !tiktokKey->text().isEmpty() &&
        !CredentialStore::write("TikTokManualKey", tiktokKey->text(), &error))
        QMessageBox::warning(this, "TikTok stream key", error);

}

void T0GSettingsDialog::clearTwitchKey()
{
    CredentialStore::remove("TwitchManualKey");
    twitchKey->clear();
}

void T0GSettingsDialog::clearTikTokKey()
{
    CredentialStore::remove("TikTokManualKey");
    tiktokKey->clear();
}

T0GSettings T0GSettingsDialog::settings() const
{
    T0GSettings out;
    out.autoReplay = autoReplay->isChecked();
    out.clipDuration = clipDuration->currentData().toInt();
    out.autoLoadTikTok = autoLoadTikTok->isChecked();
    out.rememberStreamInfo = rememberStreamInfo->isChecked();
    out.confirmBeforeEnd = confirmBeforeEnd->isChecked();
    out.startTwitch = startTwitch->isChecked(); out.startTikTok = startTikTok->isChecked();
    out.stopTwitch = stopTwitch->isChecked(); out.stopTikTok = stopTikTok->isChecked();
    out.preferVertical = preferVertical->isChecked();
    out.tiktokOutputTestMode = tiktokOutputTestMode->currentIndex();
    out.defaultTitle = defaultTitle->text().trimmed(); out.defaultGame = defaultGame->text().trimmed();
    out.defaultAudience = defaultAudience->currentIndex();
    out.twitchConnectionMode = twitchMode->currentIndex();
    out.twitchManualServer = twitchServer->text().trimmed();
    out.twitchClientId = QString::fromUtf8(kT0GTwitchClientId);
    out.tiktokConnectionMode = tiktokMode->currentIndex();
    out.tiktokManualServer = tiktokServer->text().trimmed();
    return out;
}
