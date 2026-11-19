#include "app.h"

#include "cli.h"
#include "instance.h"
#include "paths.h"
#include "profile.h"
#include "tray.h"
#include "window.h"

#include <QApplication>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>

namespace zs {
namespace {

const char *const kQuittingProperty = "zsQuitting";

void requestQuit()
{
    qApp->setProperty(kQuittingProperty, true);
    qApp->quit();
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
            QTextStream(stdout) << reply << Qt::endl;
            return reply.startsWith(QLatin1String("ok")) ? 0 : 1;
        }
        if (verb == QLatin1String("quit") || verb == QLatin1String("toggle"))
            return 0;
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

    lock.setHandler([&window](const QString &verb) {
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
        if (verb == QLatin1String("backup") || verb == QLatin1String("stats"))
            return QStringLiteral("err not implemented");
        return QStringLiteral("err unknown verb");
    });

    connect(&tray, &Tray::showRequested, &window, &Window::toggle);
    connect(&tray, &Tray::reloadRequested, this, [&profile] {
        profile.page()->triggerAction(QWebEnginePage::Reload);
    });
    connect(&tray, &Tray::quitRequested, this, [] { requestQuit(); });
    connect(&window, &Window::hiddenToTray, &tray, [] {});

    profile.page()->load(QUrl(QStringLiteral("https://web.whatsapp.com")));

    if (!options.hidden)
        window.showAndRaise();

    return qApp->exec();
}

} // namespace zs
