#include "app.h"

#include "cli.h"
#include "instance.h"
#include "notify.h"
#include "paths.h"
#include "profile.h"
#include "stats.h"
#include "tray.h"
#include "window.h"

#include <unistd.h>

#include <QApplication>
#include <QDateTime>
#include <QProcess>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWebEngineNotification>
#include <QWebEngineProfile>
#include <QWebEnginePage>

namespace zs {
namespace {

const char *const kQuittingProperty = "zsQuitting";

void requestQuit()
{
    qApp->setProperty(kQuittingProperty, true);
    qApp->quit();
}

QString sampleStats()
{
    const qint64 pid = QCoreApplication::applicationPid();
    const quint64 before = processTreeJiffies(pid);
    QThread::msleep(250);
    const quint64 after = processTreeJiffies(pid);
    const double hertz = double(::sysconf(_SC_CLK_TCK));
    const double cpu = hertz > 0 ? (after - before) / hertz / 0.25 * 100.0 : 0.0;
    return formatStats(processTreeRssKb(pid), cpu);
}

} // namespace

App::App(QObject *parent)
    : QObject(parent)
{
}

int App::run(const cli::Options &options)
{
    QString verb;
    if (options.quit)
        verb = QStringLiteral("quit");
    else if (options.toggle)
        verb = QStringLiteral("toggle");
    else if (options.stats)
        verb = QStringLiteral("stats");
    else if (options.backup)
        verb = QStringLiteral("backup");
    else if (!options.hidden)
        verb = QStringLiteral("show");

    InstanceLock lock(paths::socketPath());

    if (verb.isEmpty()) {
        // `--hidden` com instância já rodando: não há nada a fazer.
        QString reply;
        if (lock.send(QStringLiteral("stats"), &reply))
            return 0;
    } else {
        QString reply;
        if (lock.send(verb, &reply)) {
            const QString shown = (verb == QLatin1String("stats") && reply.startsWith(QLatin1String("ok ")))
                                      ? reply.mid(3)
                                      : reply;
            QTextStream(stdout) << shown << Qt::endl;
            return reply.startsWith(QLatin1String("ok")) ? 0 : 1;
        }
        if (verb == QLatin1String("quit") || verb == QLatin1String("toggle"))
            return 0;
        if (verb == QLatin1String("stats")) {
            QTextStream(stdout) << sampleStats() << Qt::endl;
            return 0;
        }
        if (verb == QLatin1String("stats") || verb == QLatin1String("backup")) {
            QTextStream(stderr) << "zapshell: não há instância em execução" << Qt::endl;
            return 1;
        }
    }

    QString error;
    if (!lock.acquire(&error)) {
        QTextStream(stderr) << "zapshell: " << error << Qt::endl;
        return 1;
    }

    Profile profile;
    Window window(profile.page());
    Tray tray;

    lock.setHandler([&profile, &window](const QString &verb) {
        if (verb == QLatin1String("show")) {
            window.showAndRaise();
            return QStringLiteral("ok");
        }
        if (verb == QLatin1String("toggle")) {
            window.toggle();
            return QStringLiteral("ok");
        }
        if (verb == QLatin1String("quit")) {
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            qApp->setProperty(kQuittingProperty, true);
            return QStringLiteral("ok");
        }
        if (verb == QLatin1String("backup")) {
            profile.page()->setLifecycleState(QWebEnginePage::LifecycleState::Discarded);
            qApp->setProperty(kQuittingProperty, true);
            QTimer::singleShot(0, qApp, &QCoreApplication::quit);
            return QStringLiteral("ok stopping");
        }
        if (verb == QLatin1String("stats"))
            return QStringLiteral("ok ") + sampleStats();
        return QStringLiteral("err unknown verb");
    });

    connect(&tray, &Tray::showRequested, &window, &Window::toggle);
    connect(&tray, &Tray::reloadRequested, this, [&profile] {
        profile.page()->triggerAction(QWebEnginePage::Reload);
    });
    connect(&tray, &Tray::quitRequested, this, [] { requestQuit(); });
    connect(&tray, &Tray::backupRequested, this, [] {
        QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                {QStringLiteral("--backup")});
    });
    Notifier notifier;
    connect(&window, &Window::hiddenToTray, &tray, [] {});
    connect(&window, &Window::silentToggleRequested, &tray, [&tray] { tray.toggleSilent(); });
    connect(&profile, &Profile::downloadFinished, &notifier, [&notifier](const QString &fileName) {
        notifier.show(QStringLiteral("Download concluído"), fileName);
    });

    connect(&tray, &Tray::silentToggled, &notifier, &Notifier::setSilent);
    connect(&notifier, &Notifier::clicked, &window, &Window::showAndRaise);

    profile.profile()->setNotificationPresenter(
        [&notifier](std::unique_ptr<QWebEngineNotification> notification) {
            const auto decision = decideNotify(notification->title(),
                                               QDateTime::currentMSecsSinceEpoch(),
                                               notifier.isSilent());
            if (decision == NotifyDecision::Send)
                notifier.show(notification->title(), notification->message());
            // Mesmo agrupada/descartada, a página precisa saber que foi tratada.
            notification->show();
        });

    connect(profile.page(), &QWebEnginePage::titleChanged, &tray, [&tray](const QString &title) {
        tray.setUnread(unreadFromTitle(title));
    });

    profile.page()->load(QUrl(QStringLiteral("https://web.whatsapp.com")));

    if (!options.hidden)
        window.showAndRaise();

    return qApp->exec();
}

} // namespace zs
