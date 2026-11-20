#pragma once

#include <QMainWindow>

class QLineEdit;
class QToolBar;
class QWebEnginePage;
class QWebEngineView;

namespace zs {

// Janela principal. Fechar esconde para o tray em vez de encerrar; só um
// pedido explícito de saída encerra o processo.
class Window : public QMainWindow
{
    Q_OBJECT

public:
    explicit Window(QWebEnginePage *page, QWidget *parent = nullptr);

    void showAndRaise();
    void toggle();

signals:
    void hiddenToTray();
    void silentToggleRequested();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWebEngineView *m_view = nullptr;
    QToolBar *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
};

} // namespace zs
