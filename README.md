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

`--stats` lê a última amostra de um coletor assíncrono, atualizado a cada 1 s
por `QTimer`; o handler IPC apenas consulta esse valor, sem bloquear a thread da
interface enquanto mede RSS/CPU da árvore de processos.

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

Na inicialização, os diretórios de dados, perfil e cache são criados — e os já
existentes corrigidos — com modo `0700`; o diretório de backups recebe o mesmo
modo quando um backup ou restore é feito. Cookies, IndexedDB, histórico e mídia
não ficam legíveis por outros usuários locais.

Backups são `zapshell-AAAAAMMDD-HHMMSS.tar.gz`, com retenção dos 4 mais
recentes. O app grava um `.partial`, valida o archive com `tar -tzf` e só então
publica o nome final com `rename()` atômico (POSIX, no mesmo filesystem): o
archive anterior é substituído sem deixar o caminho vazio e uma falha na
publicação preserva o backup anterior e remove o `.partial`. `--restore` recusa
nomes com `..`, absolutos, links simbólicos e qualquer membro que não seja
arquivo/diretório; a varredura do staging inclui ocultos e arquivos de sistema
(`QDir::Hidden | QDir::System`) e os tipos no archive são conferidos pela
listagem verbose do `tar`. A extração acontece em staging e o perfil anterior é
guardado como `profile.before-restore-<timestamp>`. Nunca tarre o perfil com o
app rodando: use `--backup`.

O socket IPC fica em `$XDG_RUNTIME_DIR/zapshell.sock`; a aquisição usa `flock`
em `.lock` (modo `0600`) para garantir uma única instância. Conexões que não
enviam o verbo em 2 s são desligadas, buffers acima de 1 KiB sem nova linha são
descartados e cada `send()` expira após 1500 ms por padrão.

## Notificações e permissões

O WhatsApp pede permissão de notificações na primeira execução. O app concede
somente para a origem exata `https://web.whatsapp.com` e apenas para captura de
áudio/vídeo/tela e notificações. Se o arquivo
`$XDG_DATA_HOME/zapshell/profile/.../permissions.json` tiver
`"Notifications": false`, o banner do site aparece e as notificações nativas
não chegam — é necessário conceder e recarregar (`Ctrl+R`).

Notificações repetidas do mesmo remitente em até 3 s são agrupadas; clicar na
notificação abre a janela via `ActionInvoked` (ação `default`). Downloads
concluídos geram "Download concluído" e downloads cancelados ou interrompidos
geram "Download interrompido"; em andamento, não há notificação. O agrupador
guarda no máximo 256 títulos recentes: ao atingir o teto, o mais antigo é
descartado.

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

A suíte é controlada por `ZAPSHELL_BUILD_TESTS` (ligada por padrão). Com
`-DZAPSHELL_BUILD_TESTS=OFF`, os alvos de teste não são gerados e o componente
Qt Test não é exigido na compilação.

Testes são guiless. `QT_QPA_PLATFORM=offscreen` com janela WebEngine visível
causa SIGTRAP no plugin offscreen — use a sessão Wayland para verificação visual.

Estrutura: biblioteca `zapshell_core` (CLI, paths, flags, IPC, notificações,
backup, stats) e `zapshell_ui` (janela, perfil, tray, amarração). Especificação
e plano em
`docs/superpowers/`, decisões em `.superpowers/sdd/2026-11-18-zapshell/`.

## Licença

MIT — ver `LICENSE`.
