#include <QtTest>

#include "notify.h"

class TestNotify : public QObject
{
    Q_OBJECT

private slots:
    void groupsRepeatsWithinWindow();
    void silentDrops();
    void parsesUnreadBadge();
    void notificationHasDefaultAction();
    void notificationTextIsEscaped();
};

void TestNotify::groupsRepeatsWithinWindow()
{
    QCOMPARE(zs::decideNotify(QStringLiteral("Ana"), 1000, false), zs::NotifyDecision::Send);
    QCOMPARE(zs::decideNotify(QStringLiteral("Ana"), 2500, false), zs::NotifyDecision::Group);
    QCOMPARE(zs::decideNotify(QStringLiteral("Ana"), 6000, false), zs::NotifyDecision::Send);
}

void TestNotify::silentDrops()
{
    QCOMPARE(zs::decideNotify(QStringLiteral("Bia"), 0, true), zs::NotifyDecision::Drop);
}

void TestNotify::parsesUnreadBadge()
{
    QCOMPARE(zs::unreadFromTitle(QStringLiteral("(3) WhatsApp")), 3);
    QCOMPARE(zs::unreadFromTitle(QStringLiteral("WhatsApp")), 0);
    QCOMPARE(zs::unreadFromTitle(QStringLiteral("(12) WhatsApp")), 12);
}

void TestNotify::notificationHasDefaultAction()
{
    QVERIFY(zs::notificationActions().contains(QStringLiteral("default")));
}

void TestNotify::notificationTextIsEscaped()
{
    QCOMPARE(zs::escapeNotificationText(QStringLiteral("<b>Ana</b> & <img>")),
             QStringLiteral("&lt;b&gt;Ana&lt;/b&gt; &amp; &lt;img&gt;"));
    QCOMPARE(zs::escapeNotificationText(QStringLiteral("texto normal")),
             QStringLiteral("texto normal"));
}

QTEST_GUILESS_MAIN(TestNotify)
#include "tst_notify.moc"
