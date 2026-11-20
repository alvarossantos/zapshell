#pragma once

#include <QObject>

namespace zs {

namespace cli {
struct Options;
}

// Amarra perfil, janela, tray e IPC. Retorna o código de saída do processo.
class App : public QObject
{
    Q_OBJECT

public:
    explicit App(QObject *parent = nullptr);

    int run(const cli::Options &options);
};

} // namespace zs
