#include "backup.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

namespace zs {
namespace {

bool runTar(const QStringList &arguments, QString *error, QByteArray *output = nullptr)
{
    QProcess process;
    process.start(QStringLiteral("tar"), arguments);
    if (!process.waitForFinished(120000)) {
        if (error)
            *error = QStringLiteral("tar excedeu o timeout");
        return false;
    }
    if (process.exitCode() != 0) {
        if (error) {
            const QString stderr = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = stderr.isEmpty() ? QStringLiteral("tar falhou") : stderr;
        }
        return false;
    }
    if (output)
        *output = process.readAllStandardOutput();
    return true;
}

QStringList tarLines(const QByteArray &output)
{
    QStringList lines;
    for (const QByteArray &line : output.split('\n')) {
        const QString trimmed = QString::fromLocal8Bit(line).trimmed();
        if (!trimmed.isEmpty())
            lines << trimmed;
    }
    return lines;
}

} // namespace

QString backupName(const QDateTime &now)
{
    return QStringLiteral("zapshell-") + now.toString(QStringLiteral("yyyyMMdd-HHmmss"))
           + QStringLiteral(".tar.gz");
}

QStringList backupsToDelete(const QStringList &names, int keep)
{
    QStringList sorted = names;
    sorted.sort(Qt::CaseSensitive);
    return keep >= sorted.size() ? QStringList() : sorted.mid(0, sorted.size() - keep);
}

bool createBackup(const QString &profileDir, const QString &backupDir, const QDateTime &now,
                  QString *outPath, QString *error)
{
    const QFileInfo info(profileDir);
    if (!info.isDir()) {
        if (error)
            *error = QStringLiteral("perfil não existe: ") + profileDir;
        return false;
    }
    if (!QDir().mkpath(backupDir)) {
        if (error)
            *error = QStringLiteral("não foi possível criar ") + backupDir;
        return false;
    }

    const QString archive = backupDir + "/" + backupName(now);
    const QString partial = archive + QStringLiteral(".partial");
    if (!runTar({QStringLiteral("-czf"), partial, QStringLiteral("-C"), info.absolutePath(),
                 info.fileName()},
                error)
        || !runTar({QStringLiteral("-tzf"), partial}, error)) {
        QFile::remove(partial);
        return false;
    }
    // rename(2) é atômico em POSIX no mesmo filesystem: substitui o archive
    // anterior sem deixar o caminho vazio. QFile::rename() recusa destino existente.
    const QByteArray from = QFile::encodeName(partial);
    const QByteArray to = QFile::encodeName(archive);
    if (::rename(from.constData(), to.constData()) != 0) {
        if (error)
            *error = QStringLiteral("não foi possível finalizar %1: %2")
                         .arg(archive, QString::fromLocal8Bit(std::strerror(errno)));
        QFile::remove(partial);
        return false;
    }

    QStringList backups;
    for (const QString &name : QDir(backupDir).entryList({QStringLiteral("zapshell-*.tar.gz")},
                                                         QDir::Files))
        backups << name;
    for (const QString &old : backupsToDelete(backups))
        QFile::remove(backupDir + "/" + old);

    if (outPath)
        *outPath = archive;
    return true;
}

bool snapshotArchive(const QString &source, const QString &destination, QString *error)
{
    if (!QFile::copy(source, destination)) {
        if (error)
            *error = QStringLiteral("não foi possível copiar o archive: ") + source;
        return false;
    }
    return true;
}

bool restorePreviousProfile(const QString &displaced, const QString &profileDir,
                            bool hadProfile, QString *error)
{
    if (hadProfile && !QFile::rename(displaced, profileDir)) {
        if (error)
            *error = QStringLiteral("falha ao instalar o perfil e ao devolver o anterior: ")
                     + displaced;
        return false;
    }
    return true;
}

bool validateStagedProfile(const QString &stagingPath, const QString &name, QString *error)
{
    const QString root = stagingPath + "/" + name;
    const QFileInfo rootInfo(root);
    if (!rootInfo.isDir() || rootInfo.isSymLink()) {
        if (error)
            *error = QStringLiteral("archive não contém ") + name + "/";
        return false;
    }

    QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString relative = QDir(root).relativeFilePath(path);
        if (relative == QLatin1String("."))
            continue;
        if (relative.startsWith(QLatin1String("..")) || info.isSymLink()
            || (!info.isFile() && !info.isDir())) {
            if (error)
                *error = QStringLiteral("membro inseguro no archive: ") + relative;
            return false;
        }
    }
    return true;
}

bool restoreBackup(const QString &archive, const QString &profileDir, QString *error,
                   const QDateTime &now)
{
    const QFileInfo info(profileDir);
    const QString name = info.fileName();
    const QString prefix = name + "/";
    QDir().mkpath(info.absolutePath());

    QTemporaryDir staging(info.absolutePath() + "/.zapshell-restore-XXXXXX");
    if (!staging.isValid()) {
        if (error)
            *error = QStringLiteral("não foi possível criar diretório temporário");
        return false;
    }

    const QString snapshot = staging.path() + "/archive.tar.gz";
    if (!snapshotArchive(archive, snapshot, error))
        return false;

    QStringList names;
    QByteArray output;
    if (!runTar({QStringLiteral("-tzf"), snapshot}, error, &output))
        return false;
    names = tarLines(output);

    QByteArray verbose;
    if (!runTar({QStringLiteral("-tvzf"), snapshot}, error, &verbose))
        return false;
    const QStringList types = tarLines(verbose);
    if (types.size() != names.size()) {
        if (error)
            *error = QStringLiteral("archive inconsistente");
        return false;
    }

    for (int i = 0; i < names.size(); ++i) {
        const QString &member = names.at(i);
        const QString type = types.at(i).left(1);
        if (!member.startsWith(prefix) || member.contains(QStringLiteral(".."))
            || member.startsWith(QLatin1String("/"))
            || (type != QLatin1String("-") && type != QLatin1String("d"))) {
            if (error)
                *error = QStringLiteral("membro inseguro no archive: ") + member;
            return false;
        }
    }

    if (!runTar({QStringLiteral("--no-same-owner"), QStringLiteral("--no-same-permissions"),
                 QStringLiteral("-xzf"), snapshot, QStringLiteral("-C"), staging.path()},
                error)) {
        return false;
    }
    if (!validateStagedProfile(staging.path(), name, error))
        return false;

    const QString displaced =
        profileDir + QStringLiteral(".before-restore-")
        + now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    const bool hadProfile = info.isDir();
    if (hadProfile && !QFile::rename(profileDir, displaced)) {
        if (error)
            *error = QStringLiteral("não foi possível mover o perfil atual");
        return false;
    }
    if (!QFile::rename(staging.path() + "/" + name, profileDir)) {
        if (!restorePreviousProfile(displaced, profileDir, hadProfile, error))
            return false;
        if (error)
            *error = hadProfile
                         ? QStringLiteral("não foi possível instalar o perfil restaurado; anterior em ")
                               + displaced
                         : QStringLiteral("não foi possível instalar o perfil restaurado");
        return false;
    }
    return true;
}

} // namespace zs
