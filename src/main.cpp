#include "app.h"
#include "cli.h"

#include <QApplication>
#include <QTextStream>

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

    zs::App zapshell;
    return zapshell.run(options);
}
