#include "window.h"

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QToolBar>
#include <QWebEnginePage>
#include <QWebEngineView>

namespace zs {
namespace {

const char *const kQuittingProperty = "zsQuitting";

} // namespace

Window::Window(QWebEnginePage *page, QWidget *parent)
    : QMainWindow(parent)
{
    m_view = new QWebEngineView(this);
    m_view->setPage(page);
    setCentralWidget(m_view);
    resize(1200, 800);
    setWindowTitle(QStringLiteral("zapshell"));

    m_findBar = addToolBar(QStringLiteral("Buscar"));
    m_findBar->setAllowedAreas(Qt::BottomToolBarArea);
    m_findEdit = new QLineEdit(m_findBar);
    m_findEdit->setPlaceholderText(QStringLiteral("Buscar na conversa"));
    m_findBar->addWidget(m_findEdit);
    m_findBar->hide();

    connect(m_findEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) { m_view->findText(text); });
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
    if (isVisible())
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
    if (event->matches(QKeySequence::Refresh)) {
        m_view->page()->triggerAction(QWebEnginePage::Reload);
        return;
    }
    if (event->matches(QKeySequence::Find)) {
        m_findBar->setVisible(!m_findBar->isVisible());
        if (m_findBar->isVisible())
            m_findEdit->setFocus();
        else
            m_view->findText(QString());
        return;
    }
    if (event->key() == Qt::Key_M && event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        emit silentToggleRequested();
        return;
    }

    QMainWindow::keyPressEvent(event);
}

} // namespace zs
