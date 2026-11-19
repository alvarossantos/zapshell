#include "cli.h"

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

    QTextStream(stderr) << "zapshell: ainda não implementado" << Qt::endl;
    return 1;
}
