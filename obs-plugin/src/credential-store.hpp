#pragma once
#include <QString>

class CredentialStore {
public:
    static bool write(const QString &name, const QString &secret, QString *error = nullptr);
    static QString read(const QString &name, QString *error = nullptr);
    static bool remove(const QString &name, QString *error = nullptr);
};
