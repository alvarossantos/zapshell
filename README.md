# zapshell

Cliente nativo do WhatsApp Web para Linux em C++/Qt6 WebEngine. Roda em segundo
plano no tray, notifica mensagens com o servidor de notificações do desktop e
guarda perfil, histórico e mídia em disco — sem Flatpak, sem AppImage, sem
daemon próprio.

## Requisitos

- Arch/CachyOS com `qt6-base >= 6.10`, `qt6-webengine >= 6.10`, `qt6-webchannel`.
- Sessão com `systemd --user` e servidor de notificações via DBus de sessão.
- Compilação: `cmake >= 3.21` e um compilador C++17.

## Instalação

Dentro do repositório:

```bash
makepkg -f          # gera zapshell-0.1.0-1-x86_64.pkg.tar.zst
makepkg -si         # instala — somente com sua aprovação explícita
namcap zapshell-0.1.0-1-x86_64.pkg.tar.zst
```

O `PKGBUILD` empacota o `HEAD` do repositório local (`git archive`), sem
incluir `build/` nem `spike/`. O binário instalado fica em `/usr/bin/zapshell`.

## Uso

```bash
zapshell                # abre/levanta a janela (ou inicia o app)
zapshell --hidden       # inicia oculto, apenas no tray
zapshell --toggle       # alterna mostra/oculta
zapshell --stats        # RSS da árvore de processos e % de CPU
zapshell --backup       # para o app de forma limpa, tarra e o relança oculto
zapshell --restore ARQ  # exige app parado; valida o archive antes de extrair
zapshell --quit         # encerra
```

No tray: mostrar/ocultar, recarregar, backup agora, silenciar e sair. Fechar a
janela apenas a esconde; `Ctrl+Q` pede confirmação antes de encerrar. `Ctrl+F`
abre a busca na conversa, `Ctrl+R` recarrega, `Ctrl+Shift+M` alterna silêncio.

### Autostart e atalho

Units instalados em `/usr/lib/systemd/user/`. Para subir oculto no login:

```bash
systemctl --user daemon-reload
systemctl --user enable --now zapshell.service
systemctl --user enable --now zapshell-backup.timer
```

Keybind no Hyprland/Ryoku (a sintaxe `.conf` equivalente é
`SUPER, W, exec, zapshell --toggle`). Nesta máquina o `binds.lua` usa Lua;
adicione a linha de `scripts/hyprland-keybind.lua`:

```lua
hl.bind(K(mod .. " + W"), function() hl.exec_cmd("zapshell --toggle") end)
```

Recarregar: `ryoku wm act config.reload`.

Se o app relançado pelo timer não aparecer, o ambiente gráfico não foi
importado para o `systemd --user`:

```bash
systemctl --user import-environment DISPLAY WAYLAND_DISPLAY XDG_RUNTIME_DIR DBUS_SESSION_BUS_ADDRESS
```

## Dados

| Conteúdo | Caminho |
|---|---|
| Perfil, cookies, IndexedDB, mídia | `$XDG_DATA_HOME/zapshell/profile` (padrão `~/.local/share/zapshell/profile`) |
| Cache HTTP | `$XDG_CACHE_HOME/zapshell` (padrão `~/.cache/zapshell`) |
| Backups | `$XDG_DATA_HOME/zapshell/backups` |
| Socket IPC | `$XDG_RUNTIME_DIR/zapshell.sock` |

Backups são `zapshell-AAAAAMMDD-HHMMSS.tar.gz`, com retenção dos 4 mais
recentes. `--restore` recusa nomes com `..`, absolutos e qualquer membro que não
seja arquivo/diretório, extrai em staging e guarda o perfil anterior como
`profile.before-restore-<timestamp>`. Nunca tarre o perfil com o app rodando:
use `--backup`.

## Notificações e permissões

O WhatsApp pede permissão de notificações na primeira execução. O app concede
somente para a origem exata `https://web.whatsapp.com` e apenas para captura de
áudio/vídeo/tela e notificações. Se o arquivo
`$XDG_DATA_HOME/zapshell/profile/.../permissions.json` tiver
`"Notifications": false`, o banner do site aparece e as notificações nativas
não chegam — é necessário conceder e recarregar (`Ctrl+R`).

Notificações repetidas do mesmo remitente em até 3 s são agrupadas; clicar na
notificação abre a janela via `ActionInvoked` (ação `default`).

## Limitações honestas

- **Orçamento de memória não atingido.** Medido com perfil fresco e não
  pareado: 634–976 MB RSS oculto (meta do spec: ≤ 320 MB), PSS 401–710 MB,
  CPU oculto 0–4 %. A meta **não** foi relaxada no spec; os números e as flags
  testadas estão em `docs/superpowers/notes/budget.md`. Falta medir com perfil
  pareado e conversas abertas.
- **`--disable-gpu` é padrão** por ter reduzido a amostra de pico, mas ainda
  não foi validado com uma ligação real. Se a chamada falhar, teste sem ele:
  `ZAPSHELL_FLAGS` substitui o conjunto inteiro de flags do Chromium.
- **Ligações não têm tratamento dedicado ainda.** O recebimento de mensagens
  oculto está validado; o script injetado para detectar chamada e inibir
  suspensão (M4) não foi implementado. Não prometa chamadas confiáveis enquanto
  isso não for verificado no uso real.
- **Histórico depende do pareamento.** O perfil em disco preserva histórico e
  mídia entre reinícios, mas se o servidor do WhatsApp desvincular a sessão não
  há como recuperá-la: será preciso parear de novo com o celular.
- **Compartilhamento de tela** pode não funcionar: a política de permissões é
  restritiva e o flag `--disable-gpu` pode interferir.
- **X11 não foi validado.** O smoke foi feito em Wayland/Hyprland com Qt 6.12.
- O log de console do WebEngine vai para o journald; use
  `QT_FORCE_STDERR_LOGGING=1` para vê-lo no terminal.
- `navigator.storage.persist()` é negado pelo Qt
  (`aquire-persistent-storage-denied`); o IndexedDB grava em disco, mas vale
  conferir o histórico antigo após reinícios.

## Desenvolvimento

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Testes são guiless. `QT_QPA_PLATFORM=offscreen` com janela WebEngine visível
causa SIGTRAP no plugin offscreen — use a sessão Wayland para verificação visual.

Estrutura: biblioteca `zapshell_core` (CLI, paths, flags, IPC, notificações,
backup, stats) e `zapshell_ui` (janela, perfil, tray, amarração). Especificação
e plano em
`docs/superpowers/`, decisões em `.superpowers/sdd/2026-11-18-zapshell/`.

## Licença

MIT — ver `LICENSE`.
