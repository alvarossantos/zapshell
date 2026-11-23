#include "instance.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

#include <sys/stat.h>

namespace zs {

InstanceLock::InstanceLock(QString socketPath)
    : m_socketPath(std::move(socketPath))
{
}

InstanceLock::~InstanceLock()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_socketPath);
    }
}

bool InstanceLock::acquire(QString *error)
{
    if (!m_server) {
        QLocalSocket probe;
        probe.connectToServer(m_socketPath);
        if (probe.waitForConnected(300)) {
            probe.disconnectFromServer();
            if (error)
                *error = QStringLiteral("já existe uma instância em execução");
            return false;
        }

        // Só reclamar a vaga quando a falha for recusa (socket órfão) ou o
        // arquivo já ter sumido. Timeout/permissão/arquivo regular podem ser
        // uma instância viva — remover aqui apagaria a vaga alheia.
        const bool removable = probe.error() == QLocalSocket::ConnectionRefusedError
                               || probe.error() == QLocalSocket::ServerNotFoundError;
        struct stat st{};
        const bool statOk = ::lstat(QFile::encodeName(m_socketPath).constData(), &st) == 0;
        const bool socketFile = statOk && S_ISSOCK(st.st_mode);
        if (!removable || (statOk && !socketFile)) {
            if (error)
                *error = QStringLiteral("não foi possível confirmar socket órfão: ") + m_socketPath;
            return false;
        }
        QLocalServer::removeServer(m_socketPath);

        m_server = new QLocalServer(this);
        if (!m_server->listen(m_socketPath)) {
            if (error)
                *error = QStringLiteral("não foi possível abrir %1: %2")
                             .arg(m_socketPath, m_server->errorString());
            delete m_server;
            m_server = nullptr;
            return false;
        }
        connect(m_server, &QLocalServer::newConnection, this, &InstanceLock::handleConnection);
    }
    return true;
}

bool InstanceLock::waitUntilReleased(const QString &socketPath, int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (QFile::exists(socketPath) && timer.elapsed() < timeoutMs) {
        QThread::msleep(100);
        QCoreApplication::processEvents();
    }
    return !QFile::exists(socketPath);
}

void InstanceLock::setHandler(std::function<QString(const QString &)> handler)
{
    m_handler = std::move(handler);
}

void InstanceLock::handleConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        // Cliente que conecta e nunca envia o '\n' não pode segurar a vaga.
        QTimer::singleShot(2000, socket, [socket]() { socket->disconnectFromServer(); });
        connect(socket, &QLocalSocket::readyRead, socket, [this, socket]() {
            // Verbos são curtos; não deixar um cliente local inflar o buffer
            // indefinidamente enquanto espera um '\n' que nunca vem.
            if (socket->bytesAvailable() > 1024) {
                socket->disconnectFromServer();
                return;
            }
            if (!socket->canReadLine())
                return;
            const QString verb = QString::fromUtf8(socket->readLine()).trimmed();
            const QString reply = m_handler ? m_handler(verb)
                                            : QStringLiteral("err no handler");
            socket->write(reply.toUtf8() + '\n');
            socket->flush();
            socket->disconnectFromServer();
            socket->deleteLater();
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
}

bool InstanceLock::send(const QString &verb, QString *reply, int timeoutMs)
{
    QLocalSocket socket;
    QByteArray buffer;
    connect(&socket, &QLocalSocket::readyRead, &socket, [&socket, &buffer]() {
        buffer += socket.readAll();
    });

    socket.connectToServer(m_socketPath);
    if (!socket.waitForConnected(timeoutMs))
        return false;

    socket.write(verb.toUtf8() + '\n');
    socket.flush();

    QElapsedTimer timer;
    timer.start();
    while (!buffer.contains('\n') && timer.elapsed() < timeoutMs) {
        socket.waitForReadyRead(10);
        // O dono do socket pode estar nesta mesma thread (testes, --toggle
        // dentro do próprio app): sem isto a resposta nunca seria processada.
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }

    socket.disconnectFromServer();

    if (!buffer.contains('\n'))
        return false;

    if (reply)
        *reply = QString::fromUtf8(buffer.left(buffer.indexOf('\n')));
    return true;
}

} // namespace zs
