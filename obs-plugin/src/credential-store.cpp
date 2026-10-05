#include "credential-store.hpp"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString target(const QString &name)
{
    return "T0G.StreamControl." + name;
}
}

bool CredentialStore::write(const QString &name, const QString &secret, QString *error)
{
#ifdef Q_OS_WIN
    const std::wstring targetName = target(name).toStdWString();
    const std::wstring value = secret.toStdWString();

    CREDENTIALW credential{};
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(targetName.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(value.size() * sizeof(wchar_t));
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<wchar_t *>(value.data()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(L"T0G Stream Control");

    if (!CredWriteW(&credential, 0)) {
        if (error) *error = QString("Windows Credential Manager error %1").arg(GetLastError());
        return false;
    }
    return true;
#else
    Q_UNUSED(name)
    Q_UNUSED(secret)
    if (error) *error = "Secure credential storage is currently implemented for Windows builds.";
    return false;
#endif
}

QString CredentialStore::read(const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    const std::wstring targetName = target(name).toStdWString();
    if (!CredReadW(targetName.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        const DWORD code = GetLastError();
        if (code != ERROR_NOT_FOUND && error)
            *error = QString("Windows Credential Manager error %1").arg(code);
        return {};
    }

    const auto *chars = reinterpret_cast<const wchar_t *>(credential->CredentialBlob);
    const int count = static_cast<int>(credential->CredentialBlobSize / sizeof(wchar_t));
    const QString value = QString::fromWCharArray(chars, count);
    CredFree(credential);
    return value;
#else
    Q_UNUSED(name)
    if (error) *error = "Secure credential storage is currently implemented for Windows builds.";
    return {};
#endif
}

bool CredentialStore::remove(const QString &name, QString *error)
{
#ifdef Q_OS_WIN
    const std::wstring targetName = target(name).toStdWString();
    if (!CredDeleteW(targetName.c_str(), CRED_TYPE_GENERIC, 0)) {
        const DWORD code = GetLastError();
        if (code != ERROR_NOT_FOUND) {
            if (error) *error = QString("Windows Credential Manager error %1").arg(code);
            return false;
        }
    }
    return true;
#else
    Q_UNUSED(name)
    if (error) *error = "Secure credential storage is currently implemented for Windows builds.";
    return false;
#endif
}
