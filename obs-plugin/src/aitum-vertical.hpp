#pragma once
#include <QString>

class AitumVertical {
public:
    bool available() const;
    bool configureTikTok(const QString &server, const QString &key, QString *error = nullptr);
    bool startTikTok(QString *error = nullptr);
    bool stopTikTok(QString *error = nullptr);
    bool active() const;

    static constexpr const char *outputName() { return "T0G TikTok"; }

private:
    bool callOutputCommand(const char *procedure, QString *error) const;
};
