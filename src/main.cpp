#include "app.h"
#include "backup.h"
#include "cli.h"
#include "instance.h"
#include "paths.h"
#include "web.h"

#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QProcess>
#include <QTextStream>

#include <algorithm>

namespace {

constexpr int kMaintenanceBudgetMs = 15000;

int remainingBudgetMs(const QDeadlineTimer &deadline)
{
    const qint64 remaining = deadline.remainingTime();
    if (remaining < 0)
        return 0;
    return int(std::min<qint64>(remaining, kMaintenanceBudgetMs));
}

int backupFlow()
{
    const QString socket = zs::paths::socketPath();
    const QDeadlineTimer budget(kMaintenanceBudgetMs);
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
            const int waitRemaining = remainingBudgetMs(budget);
            if (waitRemaining <= 0) {
                QTextStream(stderr)
                    << "zapshell: orçamento de manutenção esgotado (" << kMaintenanceBudgetMs
                    << " ms); backup abortado sem acessar o perfil" << Qt::endl
                    << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
                return 1;
            }
            if (!zs::InstanceLock::waitUntilReleased(socket, waitRemaining)) {
                QTextStream(stderr)
                    << "zapshell: instância não encerrou a tempo (" << kMaintenanceBudgetMs
                    << " ms); backup abortado" << Qt::endl
                    << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
                return 1;
            }
        }
        const int lockRemaining = remainingBudgetMs(budget);
        if (lockRemaining <= 0) {
            QTextStream(stderr)
                << "zapshell: orçamento de manutenção esgotado (" << kMaintenanceBudgetMs
                << " ms); backup abortado sem acessar o perfil" << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
            return 1;
        }
        if (!lock.acquireMaintenance(&error, lockRemaining)) {
            QTextStream(stderr)
                << "zapshell: falha ao adquirir lock de manutenção (" << kMaintenanceBudgetMs
                << " ms): " << error << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
            relaunch = false;
        } else {
            // Só é seguro reiniciar após tomar posse exclusiva.
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
    const QDeadlineTimer budget(kMaintenanceBudgetMs);
    QString error;
    bool restored = false;
    {
        zs::InstanceLock lock(socket);
        QString reply;
        if (lock.send(QStringLiteral("stats"), &reply)) {
            QTextStream(stderr) << "zapshell: Pare o zapshell antes de restaurar" << Qt::endl;
            return 1;
        }
        const int waitRemaining = remainingBudgetMs(budget);
        if (waitRemaining <= 0) {
            QTextStream(stderr)
                << "zapshell: orçamento de manutenção esgotado (" << kMaintenanceBudgetMs
                << " ms); restore abortado sem acessar o perfil" << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
            return 1;
        }
        if (!zs::InstanceLock::waitUntilReleased(socket, waitRemaining)) {
            QTextStream(stderr)
                << "zapshell: socket ainda presente após " << kMaintenanceBudgetMs
                << " ms; restore abortado" << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
            return 1;
        }
        const int lockRemaining = remainingBudgetMs(budget);
        if (lockRemaining <= 0) {
            QTextStream(stderr)
                << "zapshell: orçamento de manutenção esgotado (" << kMaintenanceBudgetMs
                << " ms); restore abortado sem acessar o perfil" << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
            return 1;
        }
        if (!lock.acquireMaintenance(&error, lockRemaining)) {
            QTextStream(stderr)
                << "zapshell: falha ao adquirir lock de manutenção (" << kMaintenanceBudgetMs
                << " ms): " << error << Qt::endl
                << "zapshell: manutenção abortada; relance manualmente se necessário" << Qt::endl;
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
