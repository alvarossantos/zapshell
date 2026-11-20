#pragma once

#include <QString>

namespace zs::cli {

struct Options
{
    bool hidden = false;
    bool quit = false;
    bool toggle = false;
    bool stats = false;
    bool backup = false;
    bool restore = false;
    bool version = false;
    QString restorePath;
};

// Retorna false com `error` preenchido em pt-BR para flag desconhecida ou
// argumento faltando. Nunca altera `out` parcialmente em caso de erro.
bool parseArgs(int argc, char **argv, Options &out, QString &error);

} // namespace zs::cli
