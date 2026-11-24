#pragma once

#include <QObject>
#include <QString>

#include <functional>

class QLocalServer;

namespace zs {

// Instância única do app, com IPC por socket Unix.
//
// Primeira execução: acquire() cria o servidor e devolve true.
// Segunda execução: acquire() devolve false e send() fala com a primeira.
//
// Protocolo: uma linha por mensagem, `verb\n` do cliente, `resposta\n` do
// servidor. Sem handler instalado a resposta é "err no handler".
class InstanceLock : public QObject
{
    Q_OBJECT

public:
    explicit InstanceLock(QString socketPath);
    ~InstanceLock();

    InstanceLock(const InstanceLock &) = delete;
    InstanceLock &operator=(const InstanceLock &) = delete;

    // true se esta execução é a dona do socket; false se já existe dona (ou
    // falhou, com `error` preenchido). Idempotente para o mesmo modo.
    // Transições manutenção<->aplicação no mesmo objeto são rejeitadas sem
    // liberar o descritor original.
    bool acquire(QString *error = nullptr, int waitMs = 0);
    bool acquireMaintenance(QString *error = nullptr, int waitMs = 0);

    // Envia um verbo para a instância dona. Bloqueia até a resposta ou timeout.
    bool send(const QString &verb, QString *reply = nullptr, int timeoutMs = 1500);

    // Espera até o socket ser liberado. Falso se o timeout vencer — chamadores
    // que dependem do app parado devem abortar, não seguir em frente.
    static bool waitUntilReleased(const QString &socketPath, int timeoutMs);

    void setHandler(std::function<QString(const QString &verb)> handler);

private:
    bool lockAcquire(QString *error, int waitMs);
    void lockRelease();
    void handleConnection();

    QString m_socketPath;
    QLocalServer *m_server = nullptr;
    int m_lockFd = -1;
    std::function<QString(const QString &)> m_handler;
};
} // namespace zs
