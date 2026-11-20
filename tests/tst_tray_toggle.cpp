#include <QtTest>

#include "window.h"

class TestTrayToggle : public QObject
{
    Q_OBJECT

private slots:
    void togglesVisibilityWithoutFocus();
};

void TestTrayToggle::togglesVisibilityWithoutFocus()
{
    zs::Window window(nullptr);
    window.show();
    QVERIFY(window.isVisible());
    QVERIFY(!window.isActiveWindow()); // offscreen/Wayland: foco não está na janela

    window.toggle();
    QVERIFY2(!window.isVisible(), "Mostrar/Ocultar deve esconder uma janela visível");

    window.toggle();
    QVERIFY(window.isVisible());
}

QTEST_MAIN(TestTrayToggle)
#include "tst_tray_toggle.moc"
