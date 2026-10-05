#pragma once
#include <QString>
#include <QStringList>
#include "t0g-settings.hpp"

struct PreflightResult {
    bool ok = true;
    QStringList errors;
    QString message() const { return errors.join("\n"); }
};

class PreflightCheck {
public:
    static PreflightResult run(const T0GSettings &settings, bool twitchSelected, bool tiktokSelected,
                               const QString &title, bool tiktokTokenLoaded, bool aitumAvailable);
};
