#include "stream-preset.hpp"
#include <QSettings>

static QSettings presets() { return QSettings("T0G", "T0G Stream Control Presets"); }

QStringList StreamPreset::names() {
    auto s = presets(); s.beginGroup("presets"); const auto n = s.childGroups(); s.endGroup(); return n;
}
StreamPreset StreamPreset::load(const QString &name) {
    auto s = presets(); s.beginGroup("presets/" + name); StreamPreset p; p.name=name;
    p.title=s.value("title").toString(); p.game=s.value("game").toString(); p.audience=s.value("audience",0).toInt();
    p.twitch=s.value("twitch",true).toBool(); p.tiktok=s.value("tiktok",true).toBool(); return p;
}
void StreamPreset::save() const {
    if (name.trimmed().isEmpty()) return; auto s=presets(); s.beginGroup("presets/"+name.trimmed());
    s.setValue("title",title); s.setValue("game",game); s.setValue("audience",audience);
    s.setValue("twitch",twitch); s.setValue("tiktok",tiktok); s.endGroup(); s.sync();
}
void StreamPreset::remove(const QString &name) { auto s=presets(); s.remove("presets/"+name); s.sync(); }
