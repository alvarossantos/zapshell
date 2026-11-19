#include "window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QMessageBox>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace zs {
namespace {

const char *const kQuittingProperty = "zsQuitting";

} // namespace

Window::Window(QWebEnginePage *page, QWidget *parent)
    : QMainWindow(parent)
{
    auto *view = new QWebEngineView(this);
    view->setPage(page);
    setCentralWidget(view);
    resize(1200, 800);
    setWindowTitle(QStringLiteral("zapshell"));
}

void Window::showAndRaise()
{
    show();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    raise();
    activateWindow();
}

void Window::toggle()
{
    if (isVisible() && isActiveWindow())
        hide();
    else
        showAndRaise();
}

void Window::closeEvent(QCloseEvent *event)
{
    if (qApp->property(kQuittingProperty).toBool()) {
        event->accept();
        return;
    }

    hide();
    emit hiddenToTray();
    event->ignore();
}

void Window::keyPressEvent(QKeyEvent *event)
{
    if (event->matches(QKeySequence::Quit)) {
        if (QMessageBox::question(this, QStringLiteral("zapshell"),
                                  QStringLiteral("Encerrar o zapshell? As mensagens novas não "
                                                 "chegam enquanto ele estiver fechado."))
            == QMessageBox::Yes) {
            qApp->setProperty(kQuittingProperty, true);
            qApp->quit();
        }
        return;
    }

    QMainWindow::keyPressEvent(event);
}

} // namespace zs
