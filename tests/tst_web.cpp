#include <QtTest>

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
};

void TestWeb::flagsHaveBudgetDefaults()
{
    const QString flags = zs::web::chromiumFlags(QString());

    QVERIFY(flags.contains("--renderer-process-limit=1"));
    QVERIFY(flags.contains("--max-old-space-size=384"));
    QVERIFY(flags.contains("--disable-features=Translate,MediaRouter,OptimizationHints"));
    QVERIFY(flags.contains("--disable-background-networking"));
    QVERIFY(!flags.contains("--disable-gpu"));
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

QTEST_GUILESS_MAIN(TestWeb)
#include "tst_web.moc"
