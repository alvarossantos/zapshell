#pragma once

#include <QString>

namespace zs::web {

// Flags do Chromium. `overrides` vazio devolve o conjunto padrão do orçamento;
// não-vazio substitui os padrões por inteiro (escape hatch via ZAPSHELL_FLAGS).
QString chromiumFlags(const QString &overrides);

// Remove os tokens QtWebEngine/x e Qt/x do User-Agent, que fazem o WhatsApp Web
// tratar o cliente como navegador não suportado.
QString normalizeUserAgent(QString userAgent);

// Origem exata (`scheme://host[:port]`) recebe permissão apenas para captura de
// áudio/vídeo/tela e notificações. Qualquer outra origem ou feature é negada.
bool permissionGranted(const QString &origin, int feature);

} // namespace zs::web
