# zapshell — cliente WhatsApp nativo para Linux (design)

Data: 2026-11-18
Status: aguardando aprovação

## 1. Intenção e porquê

Usar WhatsApp no desktop sem as dores das alternativas atuais:

- **zapzap** (Flatpak/PyQt6 + QtWebEngine): estável, mas o Flatpak duplica runtime
  inteiro em disco e adiciona overhead de sandbox.
- **zapfast** (AppImage, binário único de 75 MB): leve em disco, mas recria/recarrega
  sessão, atrasa sincronização e não segura o pareamento.

O objetivo é um app que rode no motor do navegador que **já está no sistema**
(`qt6-webengine`), mantendo a sessão permanentemente viva e o histórico durável —
com ligações funcionando, porque o usuário precisa receber chamadas com o celular
bloqueado ou longe.

Fatos que moldam o projeto (verificados nesta máquina, CachyOS, Hyprland + iNiR shell):

1. `qt6-webengine 6.140.0` (Qt 6.10) já instalado; headers têm
   `QWebEngineProfile::setNotificationPresenter`, `QWebEnginePage::LifecycleState`,
   `QWebEngineSettings::ScreenCaptureEnabled`, `QWebEngineDownloadRequest`.
2. Existe host StatusNotifier (`inir-shell`) e `org.freedesktop.Notifications` na
   sessão → tray e notificações nativas são viáveis.
3. WhatsApp multi-dispositivo entrega mensagens novas com o celular desligado.
4. **Histórico é empurrado pelo celular no pareamento.** Nenhum cliente (nem o
   oficial) puxa histórico antigo do servidor. Logo, a garantia é: nunca perder
   pareamento + nunca perder o cache local + backup.
5. Chamadas em dispositivo vinculado exigem WebRTC classe Chromium.

## 2. Objetivos

- G1 Receber mensagens em tempo real com celular bloqueado/desligado.
- G2 Receber e fazer ligações de voz (e vídeo se o engine permitir).
- G3 Pareamento que sobrevive a reboot, atualização do app e crash, sem reescanear QR.
  (Não cobre desvínculo decidido pelo servidor do WhatsApp: nesse caso o app detecta
  e avisa — ver 7.8.)
- G4 Histórico de mensagens, áudios, fotos e arquivos preservado localmente, com backup.
- G5 Consumo menor que zapzap-flatpak e startup rápido com perfil quente.
- G6 Integração de desktop: notificações nativas, tray, autostart, atalho global opcional.

## 3. Não-objetivos (v1)

- Múltiplas contas / múltiplos perfis simultâneos.
- Publicação no AUR (v1 instala via `makepkg -si` local).
- Temas, plugins, extensões ou modificação da UI do WhatsApp Web.
- Criptografia adicional do armazenamento local (fica a cargo do disco do usuário).
- Compartilhamento de tela em chamada, se o engine não suportar de forma estável.

## 4. Anti-objetivos (falha mesmo funcionando)

- Virar wrapper Electron/CEF bundlado (>150 MB, o problema do zapzap/zapfast).
- Bifurcar zapzap ou zapfast.
- Perseguir leveza ao custo de perder o requisito de ligação.
- Perder pareamento para economizar memória (ex.: limpar perfil ao sair).

## 5. Restrições

- `web.whatsapp.com` exige User-Agent de navegador suportado; o UA padrão do
  QtWebEngine contém `QtWebEngine/x` → override obrigatório, mantendo o número do
  Chromium do engine.
- Sem Flatpak/Snap: linkar `qt6-webengine` do sistema.
- Wayland/Hyprland: permissões de microfone/câmera vêm de
  `QWebEnginePage::featurePermissionRequested`, não do portal.
- Conteúdo da página é dado não confiável; nada dele altera configuração do app.

## 6. Abordagens consideradas

| | A — Shell Qt6 WebEngine | B — GTK4 + WebKitGTK 2.54 | C — Daemon whatsmeow (Go) | D — Híbrido |
|---|---|---|---|---|
| Ligação | provável (Chromium 140) | incerto (WebRTC GStreamer) | inexistente | sim (janela sob demanda) |
| Mensagem com celular off | sim | sim | sim, mais rápido | sim |
| Pareamento eterno | perfil persistente | perfil persistente | chaves em SQLite | sim |
| Histórico/mídia | cache do engine | cache do engine | SQLite próprio | SQLite + cache |
| RSS idle | 250–350 MB | 250–350 MB | 40–80 MB | 40–80 MB parado |
| Esforço | baixo/médio | baixo | alto | alto |

**Escolhida: A.** B foi descartada porque o requisito de ligação é duro e o WebRTC
do WebKitGTK não é validado pelo WhatsApp. C não tem stack de voz. D resolve o RSS,
mas dobra o esforço e o risco; fica no backlog como evolução (o shell projetado aqui
não impede migrar o transporte para um daemon depois).

## 7. Decisões de arquitetura

### 7.1 Stack

- C++17, Qt6 Widgets + QtWebEngineWidgets (sem QML: menos RSS e menos peças).
- CMake + `pkg-config` para Qt6WebEngineWidgets.
- Sem dependências de terceiros. Libnotify via `QDBusInterface` em
  `org.freedesktop.Notifications` (sem libnotify-dev).

### 7.2 Perfil e durabilidade (G3, G4)

- Perfil **nomeado e persistente** em `$XDG_DATA_HOME/zapshell/profile`
  (nunca em `~/.cache`, que é limpo por ferramentas do sistema).
- `PersistentCookiesPolicy = ForcePersistentCookies`,
  `HttpCacheType = DiskHttpCache`, `HttpCacheMaximumSize = 512 MB`.
- Conceder `PersistentStorage` para `https://web.whatsapp.com` de forma
  programática (`QWebEngineProfile::setPersistentStoragePath` + permissão nas
  requisições), para IndexedDB/armazenamento de mídia não ser tratado como efêmero.
- **Nunca** limpar cache/storage automaticamente; nenhuma opção de "limpar dados".
- Backup: `--backup` é **verbo IPC**, não tarball ao vivo. O processo em execução
  encerra a página de forma limpa (flush do storage), sai, o wrapper tarra
  `profile/` em `$XDG_DATA_HOME/zapshell/backups/` (retenção 4, nome com timestamp) e
  reinicia o app se ele estava rodando. Perfil vivo com LevelDB/SQLite não é
  consistente em cópia concorrente. `--restore <arquivo>` faz o inverso com o app
  parado e guarda o perfil atual antes de sobrescrever.
  Disparo: timer systemd semanal, item de menu, ou CLI.

### 7.3 Ciclo de vida e integração de desktop (G6)

- `main()`: instância única via `QLocalServer`; segunda execução envia "show" ao
  socket e sai.
- `QSystemTrayIcon` (StatusNotifier): Mostrar/Ocultar, Recarregar página,
  Backup agora, Silenciar notificações, Sair. Fechar a janela = esconder
  (só sai pelo menu ou `--quit`).
- Autostart: `systemd --user` `zapshell.service` (`--hidden`) + `.desktop`
  para lançamento manual.
- Título/tooltip mostram contagem de não lidos.

### 7.4 Notificações (G1)

- `QWebEngineProfile::setNotificationPresenter` → notificação nativa via DBus, com
  título, corpo e ícone.
- **Com a janela oculta, ligação entrante: toca o som da notificação, mostra o
  indicador no tray e NÃO rouba foco.** A janela só sobe quando o usuário clica na
  notificação ou no tray. Consequência de projeto: nada de `hyprctl dispatch
  focuswindow` no caminho de chegada de chamada; ele fica só no clique.
- Modo silencioso respeitado (não envia ao DBus; só atualiza contador).
- Notificações duplicadas na mesma janela de tempo são agrupadas.

### 7.5 Ligações (G2)

- `featurePermissionRequested` para `web.whatsapp.com`: conceder
  `MediaAudioCapture`/`MediaVideoCapture` (e `MediaAudioVideoCapture`) quando a
  origem for exatamente o WhatsApp; negar para qualquer outra origem.
- `ScreenCaptureEnabled` ligado; se `getDisplayMedia` falhar, degradar sem quebrar
  a chamada de voz.
- Estado de chamada vem do script injetado (7.8): início, fim e tipo. A inibição de
  suspensão (`org.freedesktop.login1` `Inhibit` com `what="idle"`) existe **apenas**
  entre "chamada iniciou" e "chamada terminou", com timeout de segurança de 4 h e
  liberação em qualquer saída anormal do app. Se o estado é desconhecido, não inibe.
- Indicador de chamada ativa no tray enquanto o estado durar.

### 7.8 Política de script injetado

Aceito e restrito. O script:

- é **bundled no binário**, versionado junto com o app, uma versão por release;
- faz apenas dois trabalhos: (a) emitir eventos de estado de chamada (início/fim/tipo)
  e (b) sinalizar quando a página está na tela de QR/pareamento;
- **não lê conteúdo de conversa**, não faz rede, não envia nada para fora do app,
  não altera nada além de hooks de `getUserMedia`/`RTCPeerConnection` e um
  `MutationObserver` limitado à raiz do app do WhatsApp;
- falha aberto: se o WhatsApp mudar o DOM e o script parar de emitir, o app segue
  funcionando sem indicador e sem inibição — nunca quebra o uso;
- é reinstalado em cada carregamento (`QWebEngineScript`, `InjectionPoint::DocumentReady`,
  mundo isolado, sem acesso a `unsafeWindow`).

Isso não conflita com §5: a regra é que conteúdo de página é dado e não muda
configuração; o script é código nosso, versionado, com superfície declarada.

### 7.6 Orçamento de recursos (critério de aceite, G5)

| Métrica | Alvo |
|---|---|
| RSS total idle, janela visível, 3 conversas | ≤ 350 MB |
| RSS total idle, oculto | ≤ 320 MB |
| Pico RSS em chamada de vídeo | ≤ 1.1 GB |
| Startup perfil quente até chat visível | ≤ 2.5 s |
| CPU idle oculto | ≤ 2% de 1 núcleo |
| Tamanho instalado (fora deps do sistema) | ≤ 5 MB |

Flags padrão (sobrescrevíveis por `ZAPSHELL_FLAGS`):
`--renderer-process-limit=1 --js-flags=--max-old-space-size=384
--disable-features=Translate,MediaRouter,OptimizationHints
--disable-background-networking`. `--disable-gpu` **não** é padrão; entra apenas se
a medição mostrar ganho e chamada continuar funcional (vídeo em software custa CPU).

`zapshell --stats` imprime RSS/CPU do processo e filhos lendo `/proc`, para medir
sem instrumentação externa.

### 7.7 Outros comportamentos

- Downloads: `QWebEngineDownloadRequest` → `~/Downloads`, sufixo em colisão,
  notificação ao concluir.
- **Uploads em escopo**: seletor de arquivo disparado pela página, arrastar-e-soltar
  do gerenciador de arquivos e colar imagem do clipboard. Aceite próprio no M5 —
  é o caminho mais frágil no Wayland e falha silenciosa aqui é inaceitável no uso
  diário.
- Atalhos internos: `Ctrl+R` recarrega, `Ctrl+F` busca, `Ctrl+Shift+M` silencia,
  `Ctrl+Q` sai (com confirmação "o pareamento é preservado").
- Atalho global: **fragmento de keybind do Hyprland** chamando `zapshell --toggle`
  (segunda execução fala com o socket e alterna mostrar/ocultar). Sem portal
  GlobalShortcuts, sem dependência nova; o PKGBUILD imprime o fragmento no
  pós-instalação.
- CLI: `--version`, `--stats`, `--backup`, `--restore <arquivo>`, `--toggle`,
  `--hidden`, `--quit`.

### 7.9 Versão mínima de plataforma

Alvo é **Qt ≥ 6.10 / qt6-webengine ≥ 6.10**, sem matriz de versões nem guards de
compatibilidade: `find_package(Qt6 REQUIRED COMPONENTS WebEngineWidgets)` e o
PKGBUILD declaram a versão mínima e falham alto. O perfil não promete compatibilidade
com Qt anterior. Wayland/Hyprland é o alvo testado; X11 é "deve funcionar, não
validado".

## 8. Milestones

- **M0 — gate de risco WebRTC.** Spike de um arquivo: janela Qt WebEngine carregando
  `web.whatsapp.com`, parear, receber uma ligação de voz real, com a janela visível e
  depois oculta/minimizada. **Critério: binário.** Passou → segue. Falhou → paro e
  volto ao usuário com as opções medidas (aceitar sem chamada / Chromium externo com
  segundo pareamento / reavaliar D). Nenhum fallback é construído especulativamente.
  Spike é descartável; não vira código de produção.
- **M1** Shell mínimo: janela, perfil persistente, UA, single-instance, tray,
  `--toggle`.
- **M2** Notificações nativas + contador de não lidos + modo silencioso + política
  "não rouba foco".
- **M3** Pareamento/histórico: verificação de persistência após reboot e crash,
  script de detecção de tela de QR/repareamento, backup `--backup`/`--restore`
  testado com restauração real em perfil descartável.
- **M4** Ligações: permissões, script injetado de estado de chamada, indicador no
  tray, inibição de suspensão com timeout de segurança.
- **M5** Downloads, uploads (file picker, drag-drop, paste), atalhos, `--stats`,
  ajuste de flags até bater o orçamento.
- **M6** Empacotamento: `PKGBUILD` local, `.desktop`, ícone, systemd unit + timer,
  README com limitações conhecidas.

**Verificação:** checklists manuais por milestone (rodadas por você) + teste
automatizado pequeno de persistência de perfil offscreen. WhatsApp Web atrás de login
real não é automatizável de forma significativa — teste de integração de UI fica
manual por decisão. "Sobrevive a atualização do Qt" é exercício manual agendado, não
teste dentro do M3, e **nenhuma operação de pacman/downgrade é feita sem aprovação
explícita por operação**.

## 9. Riscos

| Risco | Prob. | Mitigação |
|---|---|---|
| WhatsApp rejeitar UA/engine | média | UA override mantendo versão do Chromium; fallback CEF |
| WebRTC do QtWebEngine falhar em chamada | média | M0 decide antes de codar; fallback Chromium externo |
| Engine tratar storage como efêmero e perder histórico | média | permissão de persistent storage + testes de reboot + backup |
| Atualização do qt6-webengine quebrar perfil | baixa | backup pré-atualização (hook pós-pacman opcional) |
| RSS acima do alvo | média | medir em M5; se estourar, reavaliar D |
| Sessão invalidada pelo servidor (logout remoto) | média | detecção de tela de QR + notificação; `--restore` do último backup |
| Seletor de arquivo do WebEngine falhar no Wayland | média | testar em M5; se quebrar, avaliar `QT_QPA_PLATFORMTHEME`/portal antes de considerar fora de escopo |
| Ban de conta | nula | é o cliente web oficial, não cliente não oficial |

## 10. O que fica a cargo do builder

- Nome definitivo e ícone.
- Estrutura de classes/módulos e organização de CMake.
- Escolha exata de flags Chromium e valores de cache, ajustados por medição.

## 11. Fora de escopo por decisão

- Daemon whatsmeow: rejeitado no v1; registrado como evolução D.
- Suporte a X11 puro: não testado; alvo é Wayland/Hyprland.
- Portal `GlobalShortcuts`: não usado; atalho via keybind do compositor.

## 12. Decisões tomadas na revisão do design (2026-11-18)

Respostas do usuário:

1. Ligação com janela oculta **notifica e toca, sem roubar foco**; clique traz a janela.
2. **Script injetado permitido**, bundled e versionado, superfície restrita (7.8).
3. **M0 é gate binário**: falhou, o trabalho para e as opções voltam para decisão —
   nenhum fallback especulativo é construído.

Calls do builder, para conferência ao ler:

4. Backup é verbo IPC (encerra limpo → tarra → reinicia), não cópia ao vivo.
5. Sessão invalidada pelo servidor é detectada e notificada; G3 não promete vencê-la.
6. Uploads (picker, drag-drop, paste) dentro do escopo, com aceite próprio no M5.
7. Atalho global via fragmento de keybind do Hyprland, sem portal.
8. Qt ≥ 6.10 obrigatório, sem matriz de compatibilidade.
9. Verificação por checklist manual + teste offscreen de persistência; nenhuma
   mudança de pacote do sistema sem aprovação por operação.
10. Falhas do script injetado degradam sem indicador/inibição; nunca quebram o uso.
