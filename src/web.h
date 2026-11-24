#pragma once

#include <QString>

namespace zs::web {

// Flags do Chromium. `overrides` vazio devolve o conjunto padrão do orçamento;
// não-vazio substitui os padrões por inteiro (escape hatch via ZAPSHELL_FLAGS).
QString chromiumFlags(const QString &overrides);

// Remove os tokens QtWebEngine/x e Qt/x do User-Agent, que fazem o WhatsApp Web
// tratar o cliente como navegador não suportado.
QString normalizeUserAgent(QString userAgent);

// Nome final de um download: evita sobrescrita e mantém o arquivo dentro de
// `directory`, mesmo quando o nome vem da página.
QString uniqueFileName(const QString &directory, const QString &fileName);

// Origem exata (`scheme://host[:port]`) do WhatsApp Web.
bool trustedOrigin(const QString &origin);

// Origem confiável recebe permissão apenas para captura de áudio/vídeo/tela e
// notificações. Qualquer outra origem ou feature é negada.
bool permissionGranted(const QString &origin, int feature);

// isFinished() também vale para cancelado/interrompido: anunciar conclusão
// exige o estado DownloadCompleted.
bool shouldNotifyDownload(bool finished, bool completed);

// isFinished() também vale para cancelado/interrompido: anunciar interrupção
// exige um desses estados, e não DownloadCompleted.
bool shouldNotifyInterrupted(bool finished, int state);

} // namespace zs::web
