#include "app.h"
#include "backup.h"
#include "cli.h"
#include "instance.h"
#include "paths.h"

#include <QApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QTextStream>
#include <QThread>

namespace {

void waitUntilStopped(const QString &socketPath, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (QFile::exists(socketPath) && timer.elapsed() < timeoutMs) {
        QThread::msleep(100);
        QCoreApplication::processEvents();
    }
}

int backupFlow()
{
    const QString socket = zs::paths::socketPath();
    bool wasRunning = false;

    zs::InstanceLock lock(socket);
    QString reply;
    if (lock.send(QStringLiteral("backup"), &reply)) {
        wasRunning = true;
        QTextStream(stdout) << reply << Qt::endl;
        if (!reply.startsWith(QLatin1String("ok")))
            return 1;
        waitUntilStopped(socket, 15000);
    }

    QString archive;
    QString error;
    if (!zs::createBackup(zs::paths::profileDir(), zs::paths::backupDir(),
                          QDateTime::currentDateTime(), &archive, &error)) {
        QTextStream(stderr) << "zapshell: " << error << Qt::endl;
        return 1;
    }
    QTextStream(stdout) << archive << Qt::endl;

    if (wasRunning)
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--hidden")});
    return 0;
}

int restoreFlow(const QString &archive)
{
    zs::InstanceLock lock(zs::paths::socketPath());
    QString reply;
    if (lock.send(QStringLiteral("stats"), &reply)) {
        QTextStream(stderr) << "zapshell: Pare o zapshell antes de restaurar" << Qt::endl;
        return 1;
    }

    QString error;
    if (!zs::restoreBackup(archive, zs::paths::profileDir(), &error)) {
        QTextStream(stderr) << "zapshell: " << error << Qt::endl;
        return 1;
    }
    QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--hidden")});
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    zs::cli::Options options;
    QString error;
    if (!zs::cli::parseArgs(argc, argv, options, error)) {
        QTextStream(stderr) << "zapshell: " << error << Qt::endl;
        return 1;
    }

    if (options.version) {
        QTextStream(stdout) << "zapshell " << ZAPSHELL_VERSION << Qt::endl;
        return 0;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("zapshell"));
    QApplication::setApplicationVersion(QStringLiteral(ZAPSHELL_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("zapshell"));
    QApplication::setQuitOnLastWindowClosed(false);

    if (options.backup)
        return backupFlow();
    if (options.restore)
        return restoreFlow(options.restorePath);

    zs::App zapshell;
    return zapshell.run(options);
}
