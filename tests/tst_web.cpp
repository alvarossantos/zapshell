#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "web.h"
#include "webfeatures.h"

class TestWeb : public QObject
{
    Q_OBJECT

private slots:
    void flagsHaveBudgetDefaults();
    void overridesReplaceDefaults();
    void uaDropsQtTokens();
    void whatsappGetsMicButOthersDoNot();
    void grantsNotificationsOnlyForWhatsapp();
    void trustedOriginIsExact();
    void downloadNameCannotEscapeDirectory();
    void downloadNameAvoidsCollision();
    void onlyCompletedDownloadsAreReported();
};

void TestWeb::flagsHaveBudgetDefaults()
{
    const QString flags = zs::web::chromiumFlags(QString());

    QVERIFY(flags.contains("--renderer-process-limit=1"));
    QVERIFY(flags.contains("--max-old-space-size=256"));
    QVERIFY(flags.contains("--disable-features=Translate,MediaRouter,OptimizationHints"));
    QVERIFY(flags.contains("--disable-background-networking"));
    QVERIFY(flags.contains("--disable-gpu"));
}

void TestWeb::overridesReplaceDefaults()
{
    QCOMPARE(zs::web::chromiumFlags("--foo"), QString("--foo"));
}

void TestWeb::uaDropsQtTokens()
{
    QCOMPARE(zs::web::normalizeUserAgent(
                 "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                 "QtWebEngine/6.10.0 Chrome/140.0.0.0 Safari/537.36 Qt/6.10.0"),
             QString("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                     "Chrome/140.0.0.0 Safari/537.36"));
}

void TestWeb::whatsappGetsMicButOthersDoNot()
{
    QVERIFY(zs::web::permissionGranted("https://web.whatsapp.com", zs::FeatureMediaAudioCapture));
    QVERIFY(zs::web::permissionGranted("https://web.whatsapp.com", zs::FeatureMediaAudioVideoCapture));
    QVERIFY(!zs::web::permissionGranted("https://evil.example", zs::FeatureMediaAudioCapture));
    QVERIFY(!zs::web::permissionGranted("https://web.whatsapp.com.evil.example",
                                       zs::FeatureMediaAudioCapture));
    QVERIFY(!zs::web::permissionGranted("http://web.whatsapp.com", zs::FeatureMediaAudioCapture));
}

void TestWeb::grantsNotificationsOnlyForWhatsapp()
{
    QVERIFY(zs::web::permissionGranted("https://web.whatsapp.com", zs::FeatureNotifications));
    QVERIFY(!zs::web::permissionGranted("https://evil.example", zs::FeatureNotifications));
}

void TestWeb::trustedOriginIsExact()
{
    QVERIFY(zs::web::trustedOrigin(QStringLiteral("https://web.whatsapp.com")));
    QVERIFY(!zs::web::trustedOrigin(QStringLiteral("https://web.whatsapp.com/")));
    QVERIFY(!zs::web::trustedOrigin(QStringLiteral("https://evil.example")));
}

void TestWeb::downloadNameCannotEscapeDirectory()
{
    const QString dir = QDir::cleanPath(QDir::tempPath() + "/zapshell-downloads");
    const QStringList names = {
        QStringLiteral("../../etc/passwd"), QStringLiteral("/etc/passwd"),
        QStringLiteral(".."),               QStringLiteral("."),
        QStringLiteral("a/../../b"),        QStringLiteral("..\\..\\secret"),
    };
    for (const QString &name : names) {
        const QString safe = zs::web::uniqueFileName(dir, name);
        const QString target = QDir::cleanPath(dir + QLatin1Char('/') + safe);
        QVERIFY2(target.startsWith(dir + QLatin1Char('/')),
                 qPrintable(name + QStringLiteral(" -> ") + target));
    }
}

void TestWeb::downloadNameAvoidsCollision()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile existing(dir.path() + QStringLiteral("/foto.jpg"));
    QVERIFY(existing.open(QIODevice::WriteOnly));
    existing.close();

    QCOMPARE(zs::web::uniqueFileName(dir.path(), QStringLiteral("foto.jpg")),
             QStringLiteral("foto (1).jpg"));
}

void TestWeb::onlyCompletedDownloadsAreReported()
{
    QVERIFY(!zs::web::shouldNotifyDownload(false, true)); // ainda em andamento
    QVERIFY(!zs::web::shouldNotifyDownload(true, false)); // cancelado/interrompido
    QVERIFY(zs::web::shouldNotifyDownload(true, true));   // concluído
}

QTEST_GUILESS_MAIN(TestWeb)
#include "tst_web.moc"
