#pragma once
#include <QString>

struct obs_output;
struct obs_service;

class ManualRtmpOutput {
public:
    explicit ManualRtmpOutput(QString outputName);
    ~ManualRtmpOutput();

    bool configure(const QString &server, const QString &key, QString *error = nullptr);
    bool start(QString *error = nullptr);
    void stop();
    bool active() const;
    void reset();

private:
    QString name;
    obs_output *output{};
    obs_service *service{};
};
