#include "cli.h"

namespace zs::cli {

bool parseArgs(int argc, char **argv, Options &out, QString &error)
{
    Options parsed;
    error.clear();

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == "--hidden") {
            parsed.hidden = true;
        } else if (arg == "--quit") {
            parsed.quit = true;
        } else if (arg == "--toggle") {
            parsed.toggle = true;
        } else if (arg == "--stats") {
            parsed.stats = true;
        } else if (arg == "--backup") {
            parsed.backup = true;
        } else if (arg == "--version") {
            parsed.version = true;
        } else if (arg == "--restore") {
            if (i + 1 >= argc) {
                error = "faltou o caminho depois de --restore";
                return false;
            }
            parsed.restore = true;
            parsed.restorePath = QString::fromLocal8Bit(argv[++i]);
        } else {
            error = "flag desconhecida: " + arg;
            return false;
        }
    }

    out = parsed;
    return true;
}

} // namespace zs::cli
