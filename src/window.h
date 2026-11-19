#pragma once

#include <QMainWindow>

class QWebEnginePage;

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

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

} // namespace zs
