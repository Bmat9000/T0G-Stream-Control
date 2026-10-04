#include "t0g-settings.hpp"
#include "credential-store.hpp"

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
#include <QVBoxLayout>

namespace {
QSettings store() { return QSettings("T0G", "T0G Stream Control"); }
}

T0GSettings T0GSettings::load()
{
    QSettings s = store();
    T0GSettings out;
    out.autoLoadTikTok = s.value("tiktok/autoLoad", true).toBool();
    out.rememberStreamInfo = s.value("stream/rememberInfo", true).toBool();
    out.confirmBeforeEnd = s.value("stream/confirmEnd", true).toBool();
    out.startTwitch = s.value("platforms/startTwitch", true).toBool();
    out.startTikTok = s.value("platforms/startTikTok", true).toBool();
    out.stopTwitch = s.value("platforms/stopTwitch", true).toBool();
    out.stopTikTok = s.value("platforms/stopTikTok", true).toBool();
    out.preferVertical = s.value("video/preferVertical", true).toBool();
    out.defaultTitle = s.value("defaults/title").toString();
    out.defaultGame = s.value("defaults/game", "Rainbow Six Siege").toString();
    out.defaultAudience = s.value("defaults/audience", 0).toInt();
    out.twitchConnectionMode = s.value("accounts/twitchMode", 0).toInt();
    out.twitchManualServer = s.value("accounts/twitchServer", "rtmp://live.twitch.tv/app").toString();
    out.tiktokConnectionMode = s.value("accounts/tiktokMode", 0).toInt();
    out.tiktokManualServer = s.value("accounts/tiktokServer").toString();
    return out;
}

void T0GSettings::save() const
{
    QSettings s = store();
    s.setValue("tiktok/autoLoad", autoLoadTikTok);
    s.setValue("stream/rememberInfo", rememberStreamInfo);
    s.setValue("stream/confirmEnd", confirmBeforeEnd);
    s.setValue("platforms/startTwitch", startTwitch);
    s.setValue("platforms/startTikTok", startTikTok);
    s.setValue("platforms/stopTwitch", stopTwitch);
    s.setValue("platforms/stopTikTok", stopTikTok);
    s.setValue("video/preferVertical", preferVertical);
    s.setValue("defaults/title", defaultTitle);
    s.setValue("defaults/game", defaultGame);
    s.setValue("defaults/audience", defaultAudience);
    s.setValue("accounts/twitchMode", twitchConnectionMode);
    s.setValue("accounts/twitchServer", twitchManualServer);
    s.setValue("accounts/tiktokMode", tiktokConnectionMode);
    s.setValue("accounts/tiktokServer", tiktokManualServer);
    s.sync();
}

T0GSettingsDialog::T0GSettingsDialog(const T0GSettings &cfg, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("T0G Stream Control Settings");
    setMinimumWidth(520);
    auto *layout = new QVBoxLayout(this);

    auto *accounts = new QGroupBox("Accounts / Connection", this);
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
    twForm->addRow("Mode", twitchMode);
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

    auto *credentials = new QGroupBox("Current Stream Credentials / Fallback", this);
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

    auto *general = new QGroupBox("General", this);
    auto *generalLayout = new QVBoxLayout(general);
    autoLoadTikTok = new QCheckBox("Automatically load TikTok login from Streamlabs when OBS starts", general);
    rememberStreamInfo = new QCheckBox("Remember last stream title, game and audience", general);
    confirmBeforeEnd = new QCheckBox("Confirm before ending all selected streams", general);
    generalLayout->addWidget(autoLoadTikTok);
    generalLayout->addWidget(rememberStreamInfo);
    generalLayout->addWidget(confirmBeforeEnd);
    layout->addWidget(general);

    auto *platforms = new QGroupBox("GO LIVE / END LIVE behavior", this);
    auto *platformLayout = new QVBoxLayout(platforms);
    startTwitch = new QCheckBox("GO LIVE starts Twitch when Twitch is selected", platforms);
    startTikTok = new QCheckBox("GO LIVE starts TikTok when TikTok is selected", platforms);
    stopTwitch = new QCheckBox("END LIVE stops Twitch when Twitch is selected", platforms);
    stopTikTok = new QCheckBox("END LIVE stops TikTok when TikTok is selected", platforms);
    platformLayout->addWidget(startTwitch); platformLayout->addWidget(startTikTok);
    platformLayout->addWidget(stopTwitch); platformLayout->addWidget(stopTikTok);
    layout->addWidget(platforms);

    auto *video = new QGroupBox("TikTok video", this);
    auto *videoLayout = new QVBoxLayout(video);
    preferVertical = new QCheckBox("Prefer Aitum Vertical output when available", video);
    videoLayout->addWidget(preferVertical);
    layout->addWidget(video);

    auto *defaults = new QGroupBox("Defaults", this);
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
    defaultTitle->setText(cfg.defaultTitle); defaultGame->setText(cfg.defaultGame);
    defaultAudience->setCurrentIndex(cfg.defaultAudience);

    connect(twitchMode, &QComboBox::currentIndexChanged, this, [this] { updateAccountFields(); });
    connect(tiktokMode, &QComboBox::currentIndexChanged, this, [this] { updateAccountFields(); });
    connect(clearTwitch, &QPushButton::clicked, this, [this] { clearTwitchKey(); });
    connect(clearTikTok, &QPushButton::clicked, this, [this] { clearTikTokKey(); });
    updateAccountFields();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveSecrets(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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
    out.autoLoadTikTok = autoLoadTikTok->isChecked();
    out.rememberStreamInfo = rememberStreamInfo->isChecked();
    out.confirmBeforeEnd = confirmBeforeEnd->isChecked();
    out.startTwitch = startTwitch->isChecked(); out.startTikTok = startTikTok->isChecked();
    out.stopTwitch = stopTwitch->isChecked(); out.stopTikTok = stopTikTok->isChecked();
    out.preferVertical = preferVertical->isChecked();
    out.defaultTitle = defaultTitle->text().trimmed(); out.defaultGame = defaultGame->text().trimmed();
    out.defaultAudience = defaultAudience->currentIndex();
    out.twitchConnectionMode = twitchMode->currentIndex();
    out.twitchManualServer = twitchServer->text().trimmed();
    out.tiktokConnectionMode = tiktokMode->currentIndex();
    out.tiktokManualServer = tiktokServer->text().trimmed();
    return out;
}
