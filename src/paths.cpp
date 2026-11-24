#include "paths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <unistd.h>

namespace zs::paths {
namespace {

QString envOr(const char *variable, const QString &fallback)
{
    const QString value = qEnvironmentVariable(variable);
    return value.isEmpty() ? fallback : value;
}

QString homeFallback(const QString &suffix)
{
    return QDir::homePath() + "/" + suffix;
}

} // namespace

QString dataDir()
{
    return envOr("XDG_DATA_HOME", homeFallback(".local/share")) + "/zapshell";
}

QString profileDir()
{
    return dataDir() + "/profile";
}

QString cacheDir()
{
    return envOr("XDG_CACHE_HOME", homeFallback(".cache")) + "/zapshell";
}

QString backupDir()
{
    return dataDir() + "/backups";
}

QString socketPath()
{
    const QString fallback = "/run/user/" + QString::number(::getuid());
    return envOr("XDG_RUNTIME_DIR", fallback) + "/zapshell.sock";
}

void ensurePrivateDir(const QString &path)
{
    if (!QDir().mkpath(path))
        return;
    QFile::setPermissions(path,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}

} // namespace zs::paths
