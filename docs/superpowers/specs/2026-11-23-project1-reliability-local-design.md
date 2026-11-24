# Projeto 1 — Confiabilidade e segurança local

Data: 2026-11-23
HEAD auditado: `2dfe6a2` (local = `origin/main`)
Workflow: especificação → aprovação → plano → implementação com testes → validação e empacotamento.

## Intenção

Tornar o ZapShell confiável para uso diário ininterrupto, com endurecimento progressivo da
segurança local, **sem** alterar permissões de mídia, origens de download, flags do Chromium ou
qualquer comportamento visível ao usuário além dos bugs listados aqui.

## Goals

- Garantir uma única instância sob inicialização concorrente.
- Impedir leitura da sessão/histórico/mídia por outros usuários locais.
- Não publicar backup incompleto nem perder o perfil anterior no restore.
- Distinguir download concluído de download interrompido nas notificações.
- Manter 100% da suíte verde e o pacote reproduzível.

## Non-goals

- Não mexer em `permissionGranted`, `onPermissionRequested`, flags do Chromium ou `ZAPSHELL_FLAGS`.
- Não restringir origens de download (CDNs do WhatsApp ainda não foram medidas).
- Não tocar em PSS, `--doctor`, TOML, MPRIS, chamadas, AUR ou CI nesta etapa.
- Não alterar keybinds, ícones ou a política de notificações agrupadas.

## Estado confirmado no HEAD (auditoria de 2026-11-23)

| Item | Status | Evidência |
|---|---|---|
| Backup atômico | **Parcial** | `src/backup.cpp:75-96` — grava `.partial` e valida com `tar -tzf` (correto), mas `QFile::remove(archive)` (85) precede `QFile::rename` (91): janela sem backup definitivo. `tst_backup.cpp::stalePartialIsNotLeftBehind`, `:createAndRestoreRoundTrip`; falta teste de substituição de archive existente |
| Validação do staging | **Corrigido** | `src/backup.cpp:132-158` — `QDirIterator` com `Hidden\|System`; `tst_backup.cpp::hiddenSymlinkIsRejected`, `stagedSymlinkIsRejected` |
| Rollback do restore | **Corrigido** | `src/backup.cpp:120-130,215-233`; `tst_backup.cpp::failedRollbackIsReported`, `successfulRollbackRestoresPreviousProfile` |
| Timeout IPC ocioso | **Corrigido** | `src/instance.cpp:90` — `QTimer::singleShot(2000, ...)`; `tst_instance.cpp::idleConnectionIsClosed` |
| Limite de linha IPC | **Corrigido** | `src/instance.cpp:94-97`; `tst_instance.cpp::oversizedLineIsDropped` |
| Socket órfão / arquivo regular | **Corrigido** | `src/instance.cpp:44-54`; `tst_instance.cpp::staleSocketIsReclaimed`, `doesNotDeleteRegularFileAtSocketPath` |
| Estatísticas assíncronas | **Corrigido** | `src/app.cpp:46-78` — `StatsSampler` com `QTimer`, handler lê `latest()` |
| Notificação `DownloadCompleted` | **Corrigido** | `src/profile.cpp:101-107` + `src/web.cpp:93-96` |
| Notificação de download interrompido | **Pendente** | `src/profile.cpp:101-107` só emite `downloadFinished` |
| Concorrência na aquisição do IPC | **Pendente (race real)** | `src/instance.cpp:44-64` — janela entre `lstat` (47) e `removeServer`/`listen` (54/57). Duas auditorias independentes chegaram ao mesmo veredito |
| Permissões dos diretórios locais | **Pendente** | `src/profile.cpp:34-35`, `src/backup.cpp:69` usam `QDir().mkpath()` → 0755 com umask 0022. `stat` real: `~/.local/share/zapshell`, `.../profile` e `~/.cache/zapshell` = `drwxr-xr-x` |
| Tipos especiais no restore | **Parcial** | `src/backup.cpp:195-205` confia em listagem textual (`-`/`d`); sem teste de FIFO/device/socket no archive |
| README | **Parcial** | `README.md:79-121` não registra as correções já entregues |

## Achados a corrigir

### A0. Publicação do backup não é estritamente atômica

`createBackup()` valida o `.partial` com `tar -tzf` (`backup.cpp:80`) — correto — mas em seguida
faz `QFile::remove(archive)` (85) **antes** de `QFile::rename(partial, archive)` (91).
Uma interrupção entre as duas chamadas deixa o diretório sem o backup anterior e com apenas um
`.partial`, violando o critério "nenhum arquivo incompleto publicado".

**Alternativas:**

| Alternativa | Trade-off |
|---|---|
| `QFile::rename(partial, archive)` direto, sem `remove` prévio | Em POSIX, `rename()` sobre arquivo existente é atômico no mesmo filesystem. Remove o bloco 85-90. **Recomendada** — mais curta e atômica de fato. |
| `link()` + `unlink()` + `rename()` | Atômico, mas mais chamadas e sujo em caso de colisão de nome. |
| Manter `remove` + `rename` e aceitar a janela | Rejeitada: é exatamente o risco que o Projeto 1 quer eliminar. |

**Critério de aceitação:** com archive válido já presente, uma nova publicação substitui o
conteúdo sem deixar o caminho ausente em nenhum instante; `.partial` não sobra.

**Testes:** (1) substituição de archive existente — conteúdo antigo é substituído e nenhum
`.partial` permanece; (2) falha de publicação com destino inválido — o archive anterior permanece
intacto. A propriedade atômica em si é garantia POSIX de `rename()`, documentada em comentário
no código (não há hook determinístico para interromper entre duas syscalls num teste guiless).

### A. Race na aquisição do IPC

`acquire()` faz `probe` → `lstat` → `removeServer` → `listen()`. Duas instâncias podem intercalar:

1. A: `lstat` OK → B: `lstat` OK
2. A: `removeServer` → A: `listen` OK (A é a dona)
3. B: `removeServer` **desvincula o caminho do servidor de A** → B: `listen` OK

Resultado: A e B rodam, mas o caminho do socket aponta para B. A fica inalcançável por IPC,
violando o critério "apenas uma instância sob inicialização concorrente".

**Alternativas consideradas:**

| Alternativa | Trade-off |
|---|---|
| `flock()` em `$XDG_RUNTIME_DIR/zapshell.lock` ao redor de toda a sequência | Atômico, serializa, libc (sem dependência nova), preserva protocolo e testes atuais. Cria 1 arquivo de lock. **Recomendada.** |
| `bind()` direto + `QLocalServer::setSocketDescriptor()` | `bind` é atômico, mas ainda exige remover socket órfão antes — race menor, não eliminada. Mais código. |
| Nome abstrato do Linux (`@zapshell`) |Elimina o arquivo e o TOCTOU, mas quebra `waitUntilReleased()` (`QFile::exists`) e a detecção de órfão. Rework grande. |

**Decisão proposta:** `flock()` com `LOCK_EX|LOCK_NB` em arquivo de lock separado; se não
conseguir, devolver erro "inicialização concorrente" em vez de prosseguir. O lock é liberado no
destrutor. `waitUntilReleased()` continua checando apenas o socket (o lock é curto, só cobre `acquire()`).

**Critério de aceitação:** com N processos chamando `acquire()` simultaneamente no mesmo caminho,
exatamente 1 retorna `true`; os demais retornam `false` com erro; o socket responde ao dono.

**Teste:** `tst_instance.cpp` — novo teste com `fork()` de N filhos (ou threads com barreira)
sincronizados por `pthread_barrier`/pipe, cada um chamando `acquire()`; contar exatamente 1 sucesso.

### B. Permissões dos diretórios locais

`profileDir()` (`src/profile.cpp:34`) e `backupDir()` (`src/backup.cpp:69`) usam `QDir().mkpath()`,
que aplica `0777 & ~umask` = **0755** nesta máquina (umask 0022, confirmado por `stat`).
Qualquer usuário local pode listar e ler cookies, IndexedDB, histórico e mídia.

**Alternativas consideradas:**

| Alternativa | Trade-off |
|---|---|
| Helper `ensurePrivateDir()` em `paths.cpp`: `mkpath` + `chmod 0700` em `dataDir`, `profileDir`, `cacheDir`, `backupDir` | Correção pontual, escopo restrito aos dados do app, sem dependência. **Recomendada.** |
| `::umask(0077)` global no `main.cpp` | Afeta também Downloads e arquivos criados pelo WebEngine; efeito colateral amplo. |
| Documentar e não corrigir | Deixa a sessão legível; inaceitável para dados de mensagem. |

**Decisão proposta:** helper `ensurePrivateDir()` em `paths.cpp` — `mkpath` seguido de
`QFile::setPermissions(..., QFile::PermOwner | QFile::WriteOwner | QFile::ExeOwner)`, cobrindo
`dataDir`, `profileDir`, `cacheDir` e `backupDir`.

**Gate explícito:** aplicar `chmod` aos diretórios **já existentes** (inclusive o perfil real com
sessão ativa) altera metadados do perfil do WhatsApp. Isso **exige autorização do usuário**, pedida
em conjunto com a aprovação desta spec:

- **Autorizado:** a chamada entra na inicialização normal; diretórios novos e existentes terminam
  em `0700`; evidência final inclui `stat` dos diretórios reais.
- **Não autorizado:** o helper é usado apenas para diretórios recém-criados (perfis novos ficam
  protegidos); o perfil atual permanece `0755` e a limitação passa a constar no README. Testes
  continuam restritos a `QTemporaryDir`.

**Critério de aceitação:** em ambos os ramos, diretórios criados pelo app têm modo `0700` e os
downloads continuam funcionando. No ramo autorizado, diretórios existentes também são corrigidos.

**Teste:** `tst_paths.cpp` — criar `QTemporaryDir` com um subdiretório pré-existente em `0755`,
chamar o helper, verificar `QFileInfo::permissions()` com apenas `PermOwner`/`WriteOwner`/
`ExeOwner` (sem `PermGroup`/`PermOther`). Teste idêntico nos dois ramos.

### C. Notificação de download interrompido

`onDownloadRequested` (`src/profile.cpp:101-107`) emite apenas `downloadFinished` para
`DownloadCompleted`. `isFinished()` também vale para `Cancelled`/`Interrupted` — hoje esses
casos passam silenciosamente.

**Decisão proposta:** adicionar sinal `downloadInterrupted(const QString &fileName)`, emitido quando
`state()` for `DownloadCancelled` ou `DownloadInterrupted`, ligado a uma notificação com título
"Download interrompido". Não altera/downloads, apenas informa. Baixo risco.

**Critério de aceitação:** concluído → "Download concluído"; cancelado/interrompido →
"Download interrompido"; em andamento → nenhuma notificação.

**Teste:** estender `tst_web.cpp` com `shouldNotifyInterrupted(state)` cobrindo os quatro estados.

### D. Documentação

Atualizar `README.md` com o estado real: backup atômico, validação de ocultos/especiais,
timeout IPC, stats assíncronas, teto de 256 títulos de notificação e `ZAPSHELL_BUILD_TESTS`.
Sem alterar a seção de limitações honestas (memória/chamadas/X11 permanecem).

## Riscos e regressões possíveis

- `flock()` pode bloquear se um processo morrer segurando o lock — mitigado com `LOCK_NB` e
  liberação no destrutor; lock é curto (só `acquire()`).
- `chmod 0700` em diretório já existente pode surpreender quem compartilhava o perfil —
  impede leitura por outros usuários, que é justamente o objetivo. Sem impacto no usuário dono.
- Nova notificação pode gerar ruído se o WhatsApp cancelar downloads rotineiramente —
  monitorar após instalação; fácil de desligar.

## Fora de escopo deste projeto

Segurança web (origens/CDNs/permissões), desempenho (PSS/perfis/flags/benchmarks), multimídia,
distribuição (PKGBUILD/AUR/CI) e features do roadmap.

## Evidências exigidas ao final

- `ctest --test-dir build --output-on-failure` → 9/9 suítes, com os novos testes incluídos.
- `makepkg -f` → 100% testes no `check()`, namcap sem erros.
- Hash do binário interno comparado ao instalado antes de reiniciar.
- `stat` dos diretórios reais após a inicialização **somente no ramo autorizado**; no ramo sem
  autorização, a evidência é o teste `tst_paths.cpp` em `QTemporaryDir`.
