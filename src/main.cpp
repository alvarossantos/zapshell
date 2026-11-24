#include "app.h"
#include "backup.h"
#include "cli.h"
#include "instance.h"
#include "paths.h"
#include "web.h"

#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QProcess>
#include <QTextStream>

namespace {

int backupFlow()
{
    const QString socket = zs::paths::socketPath();
    bool wasRunning = false;
    bool relaunch = false;
    QString archive;
    QString error;
    int result = 1;
    {
        zs::InstanceLock lock(socket);
        QString reply;
        if (lock.send(QStringLiteral("backup"), &reply)) {
            QTextStream(stdout) << reply << Qt::endl;
            if (!reply.startsWith(QLatin1String("ok")))
                return 1;
            wasRunning = true;
            if (!zs::InstanceLock::waitUntilReleased(socket, 15000)) {
                QTextStream(stderr) << "zapshell: instância não encerrou a tempo (15000 ms); backup abortado" << Qt::endl;
                return 1;
            }
        }
        if (!lock.acquireMaintenance(&error, 15000)) {
            QTextStream(stderr) << "zapshell: falha ao adquirir lock de manutenção (15000 ms): " << error << Qt::endl;
        } else {
            // Só é seguro reiniciar após tomar posse exclusiva. Se outro
            // mantenedor venceu a disputa, não iniciar um perfil concorrente.
            relaunch = wasRunning;
            if (zs::createBackup(zs::paths::profileDir(), zs::paths::backupDir(),
                                    QDateTime::currentDateTime(), &archive, &error)) {
                QTextStream(stdout) << archive << Qt::endl;
                result = 0;
            } else {
                QTextStream(stderr) << "zapshell: " << error << Qt::endl;
            }
        }
    }
    if (relaunch)
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--hidden")});
    return result;
}

int restoreFlow(const QString &archive)
{
    const QString socket = zs::paths::socketPath();
    QString error;
    bool restored = false;
    {
        zs::InstanceLock lock(socket);
        QString reply;
        if (lock.send(QStringLiteral("stats"), &reply)) {
            QTextStream(stderr) << "zapshell: Pare o zapshell antes de restaurar" << Qt::endl;
            return 1;
        }
        if (!zs::InstanceLock::waitUntilReleased(socket, 15000)) {
            QTextStream(stderr) << "zapshell: socket ainda presente após 15000 ms; restore abortado" << Qt::endl;
            return 1;
        }
        if (!lock.acquireMaintenance(&error, 15000)) {
            QTextStream(stderr) << "zapshell: falha ao adquirir lock de manutenção (15000 ms): " << error << Qt::endl;
            return 1;
        }
        restored = zs::restoreBackup(archive, zs::paths::profileDir(), &error);
        if (!restored)
            QTextStream(stderr) << "zapshell: " << error << Qt::endl;
    }
    if (restored)
        QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--hidden")});
    return restored ? 0 : 1;
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

    // Precisa ser definido antes de qualquer inicialização do WebEngine.
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS",
            zs::web::chromiumFlags(qEnvironmentVariable("ZAPSHELL_FLAGS")).toLocal8Bit());

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
