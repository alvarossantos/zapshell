#include <QtTest>

#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>

#include "instance.h"

class TestInstance : public QObject
{
    Q_OBJECT

private slots:
    void secondInstanceSendsVerbToFirst();
    void unknownVerbIsRejected();
    void staleSocketIsReclaimed();
    void waitReportsWhetherSocketWasReleased();
    void oversizedLineIsDropped();
    void doesNotDeleteRegularFileAtSocketPath();
    void idleConnectionIsClosed();
};

void TestInstance::secondInstanceSendsVerbToFirst()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    QStringList received;
    first.setHandler([&received](const QString &verb) {
        received << verb;
        return QStringLiteral("ok");
    });
    QString error;
    QVERIFY2(first.acquire(&error), qPrintable(error));

    zs::InstanceLock second(socketPath);
    QVERIFY(!second.acquire()); // já existe dono

    QString reply;
    QVERIFY(second.send(QStringLiteral("toggle"), &reply));
    QCOMPARE(reply, QStringLiteral("ok"));
    QCOMPARE(received, QStringList{QStringLiteral("toggle")});
}

void TestInstance::unknownVerbIsRejected()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    first.setHandler([](const QString &verb) {
        if (verb == QLatin1String("toggle"))
            return QStringLiteral("ok");
        return QStringLiteral("err unknown verb");
    });
    QVERIFY(first.acquire());

    zs::InstanceLock client(socketPath);
    QString reply;
    QVERIFY(client.send(QStringLiteral("nuke"), &reply));
    QCOMPARE(reply, QStringLiteral("err unknown verb"));
}

void TestInstance::staleSocketIsReclaimed()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    // Socket órfão de verdade: processo filho escuta e morre com _exit, sem
    // rodar destrutor nem desvincular o arquivo.
    const pid_t pid = ::fork();
    if (pid == 0) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const QByteArray path = socketPath.toLocal8Bit();
        std::memcpy(addr.sun_path, path.constData(), size_t(path.size()));
        ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        ::listen(fd, 1);
        ::_exit(0);
    }
    QCOMPARE(::waitpid(pid, nullptr, 0), pid);
    QVERIFY(QFile::exists(socketPath));

    zs::InstanceLock next(socketPath);
    QString error;
    QVERIFY2(next.acquire(&error), qPrintable(error));
}

void TestInstance::waitReportsWhetherSocketWasReleased()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    QLocalServer server;
    QVERIFY(server.listen(socketPath));
    QVERIFY(!zs::InstanceLock::waitUntilReleased(socketPath, 150));

    server.close();
    QLocalServer::removeServer(socketPath);
    QVERIFY(zs::InstanceLock::waitUntilReleased(socketPath, 150));
}

void TestInstance::oversizedLineIsDropped()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    int calls = 0;
    first.setHandler([&calls](const QString &) {
        ++calls;
        return QStringLiteral("ok");
    });
    QVERIFY(first.acquire());

    QLocalSocket client;
    client.connectToServer(socketPath);
    QVERIFY(client.waitForConnected(1000));
    client.write(QByteArray(8192, 'x'));
    client.flush();

    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 2000);
    QCOMPARE(calls, 0);
}

void TestInstance::doesNotDeleteRegularFileAtSocketPath()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    QFile victim(socketPath);
    QVERIFY(victim.open(QIODevice::WriteOnly));
    victim.write("importante");
    victim.close();

    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(!lock.acquire(&error),
             "não deve assumir a posse apagando arquivo regular no caminho do socket");
    QVERIFY2(QFile::exists(socketPath), "arquivo regular no caminho do socket foi removido");

    QFile check(socketPath);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("importante"));
}

void TestInstance::idleConnectionIsClosed()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    int calls = 0;
    first.setHandler([&calls](const QString &) {
        ++calls;
        return QStringLiteral("ok");
    });
    QVERIFY(first.acquire());

    QLocalSocket client;
    client.connectToServer(socketPath);
    QVERIFY(client.waitForConnected(1000));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 3500);
    QCOMPARE(calls, 0);
}

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
