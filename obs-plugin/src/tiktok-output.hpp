#pragma once
#include <QString>

struct obs_output;
struct obs_service;
struct obs_encoder;

class TikTokOutput {
public:
    ~TikTokOutput();

    bool configure(const QString &server, const QString &key, QString *error = nullptr);
    bool start(QString *error = nullptr);
    void stop();
    bool active() const;
    void reset();

private:
    obs_output *output{};
    obs_service *service{};
    obs_encoder *videoEncoder{};
    obs_encoder *audioEncoder{};
};
