#include "t0g-settings.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QSettings>
#include <QVBoxLayout>

namespace {
QSettings store()
{
    return QSettings("T0G", "T0G Stream Control");
}
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
    s.sync();
}

T0GSettingsDialog::T0GSettingsDialog(const T0GSettings &cfg, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("T0G Stream Control Settings");
    setMinimumWidth(430);

    auto *layout = new QVBoxLayout(this);

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
    platformLayout->addWidget(startTwitch);
    platformLayout->addWidget(startTikTok);
    platformLayout->addWidget(stopTwitch);
    platformLayout->addWidget(stopTikTok);
    layout->addWidget(platforms);

    auto *video = new QGroupBox("TikTok video", this);
    auto *videoLayout = new QVBoxLayout(video);
    preferVertical = new QCheckBox("Prefer Aitum Vertical output when available", video);
    preferVertical->setToolTip("Reserved for the Aitum Vertical integration. Until then TikTok uses the normal OBS encoders.");
    videoLayout->addWidget(preferVertical);
    layout->addWidget(video);

    auto *defaults = new QGroupBox("Defaults", this);
    auto *form = new QFormLayout(defaults);
    defaultTitle = new QLineEdit(defaults);
    defaultGame = new QLineEdit(defaults);
    defaultAudience = new QComboBox(defaults);
    defaultAudience->addItems({"Everyone", "Mature / 18+"});
    form->addRow("Default title", defaultTitle);
    form->addRow("Default game", defaultGame);
    form->addRow("TikTok audience", defaultAudience);
    layout->addWidget(defaults);

    autoLoadTikTok->setChecked(cfg.autoLoadTikTok);
    rememberStreamInfo->setChecked(cfg.rememberStreamInfo);
    confirmBeforeEnd->setChecked(cfg.confirmBeforeEnd);
    startTwitch->setChecked(cfg.startTwitch);
    startTikTok->setChecked(cfg.startTikTok);
    stopTwitch->setChecked(cfg.stopTwitch);
    stopTikTok->setChecked(cfg.stopTikTok);
    preferVertical->setChecked(cfg.preferVertical);
    defaultTitle->setText(cfg.defaultTitle);
    defaultGame->setText(cfg.defaultGame);
    defaultAudience->setCurrentIndex(cfg.defaultAudience);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

T0GSettings T0GSettingsDialog::settings() const
{
    T0GSettings out;
    out.autoLoadTikTok = autoLoadTikTok->isChecked();
    out.rememberStreamInfo = rememberStreamInfo->isChecked();
    out.confirmBeforeEnd = confirmBeforeEnd->isChecked();
    out.startTwitch = startTwitch->isChecked();
    out.startTikTok = startTikTok->isChecked();
    out.stopTwitch = stopTwitch->isChecked();
    out.stopTikTok = stopTikTok->isChecked();
    out.preferVertical = preferVertical->isChecked();
    out.defaultTitle = defaultTitle->text().trimmed();
    out.defaultGame = defaultGame->text().trimmed();
    out.defaultAudience = defaultAudience->currentIndex();
    return out;
}
