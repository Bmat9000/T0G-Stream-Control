#pragma once
#include <obs.h>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>

namespace T0GLog {
inline QMutex &mutex() { static QMutex m; return m; }
inline QStringList &lines() { static QStringList v; return v; }

inline void write(const QString &message, int level = LOG_INFO)
{
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") + "  " + message;
    {
        QMutexLocker lock(&mutex());
        lines().append(line);
        while (lines().size() > 2000)
            lines().removeFirst();
    }
    blog(level, "[T0G Stream Control] %s", message.toUtf8().constData());
}

inline QString text()
{
    QMutexLocker lock(&mutex());
    return lines().join('\n');
}
}
