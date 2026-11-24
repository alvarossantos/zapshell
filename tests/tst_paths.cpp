#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "paths.h"

class TestPaths : public QObject
{
    Q_OBJECT

private slots:
    void profileIsNotInCache();
    void honoursXdgDataHome();
    void cacheIsSeparateFromProfile();
    void existingDirectoryBecomesPrivate();
};

void TestPaths::profileIsNotInCache()
{
    QVERIFY(!zs::paths::profileDir().contains("/.cache"));
    QVERIFY(zs::paths::profileDir().endsWith("/zapshell/profile"));
}

void TestPaths::honoursXdgDataHome()
{
    const QByteArray previous = qgetenv("XDG_DATA_HOME");
    qputenv("XDG_DATA_HOME", "/tmp/xdg-test");

    const QString data = zs::paths::dataDir();
    const QString profile = zs::paths::profileDir();

    if (previous.isEmpty())
        qunsetenv("XDG_DATA_HOME");
    else
        qputenv("XDG_DATA_HOME", previous);

    QCOMPARE(data, QString("/tmp/xdg-test/zapshell"));
    QCOMPARE(profile, QString("/tmp/xdg-test/zapshell/profile"));
}

void TestPaths::cacheIsSeparateFromProfile()
{
    QVERIFY(zs::paths::cacheDir() != zs::paths::profileDir());
    QVERIFY(zs::paths::cacheDir().endsWith("/zapshell"));
}

void TestPaths::existingDirectoryBecomesPrivate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.path() + "/zapshell/profile";
    QVERIFY(QDir().mkpath(target));
    QVERIFY(QFile::setPermissions(target,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther));
    QFileInfo before(target);
    QVERIFY(before.permissions() & QFile::ReadGroup);

    zs::paths::ensurePrivateDir(target);

    QFileInfo after(target);
    QCOMPARE(after.permissions(),
             QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                QFile::ReadUser | QFile::WriteUser | QFile::ExeUser));
    QFile probe(target + "/dados.txt");
    QVERIFY(probe.open(QIODevice::WriteOnly)); // conteúdo continua gravável
    probe.write("ok");
    probe.close();
}

QTEST_GUILESS_MAIN(TestPaths)
#include "tst_paths.moc"
