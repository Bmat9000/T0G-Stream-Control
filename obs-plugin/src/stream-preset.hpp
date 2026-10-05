#pragma once
#include <QString>
#include <QStringList>

struct StreamPreset {
    QString name;
    QString title;
    QString game;
    int audience = 0;
    bool twitch = true;
    bool tiktok = true;

    static QStringList names();
    static StreamPreset load(const QString &name);
    void save() const;
    static void remove(const QString &name);
};
