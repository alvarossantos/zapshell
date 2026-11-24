# Projeto 1 — Confiabilidade e segurança local — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fechar os quatro achados confirmados do Projeto 1 (backup atômico, race do IPC, permissões 0700, notificação de download interrompido) e documentar o estado real no README.

**Architecture:** Correções pontuais nas bibliotecas existentes. `backup.cpp` passa a publicar o archive com `rename()` atômico do POSIX. `instance.cpp` envolve `probe→lstat→remove→listen` num `flock` de curta duração. `paths.cpp` ganha helper `ensurePrivateDir()` usado por `profile.cpp`/`backup.cpp`. `web.cpp` expõe `shouldNotifyInterrupted()`, ligada a um novo sinal de `Profile`. Nenhuma dependência nova, nenhuma API pública de UI alterada.

**Tech Stack:** C++17, Qt6 (Core/Widgets/WebEngine/WebChannel), CMake ≥ 3.21, Qt Test, Arch Linux makepkg.

**Spec:** `docs/superpowers/specs/2026-11-23-project1-reliability-local-design.md`

## Global Constraints

- C++17 e Qt6 WebEngine ≥ 6.10 — sem mudança de stack.
- Arquitetura: `zapshell_core` (CLI, paths, flags, IPC, notificações, backup, stats) e `zapshell_ui` (janela, perfil, tray, amarração).
- Nenhuma dependência nova; nada de `pacman`, `yay` ou `sudo` direto — propor ao usuário.
- Não alterar keybinds, ícones, origens de download, políticas de mídia, flags do Chromium ou `ZAPSHELL_FLAGS`.
- Não modificar conteúdo do perfil real; `chmod 0700` nos diretórios existentes está **autorizado** (aprovado em 2026-11-23) e é só metadado.
- Não encerrar a instância principal nem reiniciar o app sem o "pronto" do usuário.
- `spike/` permanece untracked; nunca incluir em commits.
- Toda tarefa: teste primeiro (RED) → código mínimo (GREEN) → suíte 9/9 → commit próprio.
- Estilo de commit existente: `fix: ...` em pt-BR, imperativo.

## Review Focus

1. **Interrupção entre `remove()` e `rename()` no backup** — o leitor nunca deve ver o caminho do archive ausente. Teste: `tst_backup.cpp::existingArchiveIsReplacedWithoutGap` (Task 1).
2. **Falha de publicação com backup anterior presente** — o archive antigo deve sobreviver. Teste: `tst_backup.cpp::failedPublishKeepsPreviousArchive` (Task 1).
3. **N iniciadores simultâneos no mesmo socket** — exatamente 1 dono, os demais com erro; arquivo regular no caminho intacto. Teste: `tst_instance.cpp::concurrentAcquireHasSingleOwner` + `doesNotDeleteRegularFileAtSocketPath` (Task 2).
4. **Perfil já existente com 0755 e umask permissiva** — vira 0700 sem apagar conteúdo. Teste: `tst_paths.cpp::existingDirectoryBecomesPrivate` (Task 3).
5. **Download que termina como Cancelled/Interrupted** — notificação distinta, sem anunciar "concluído". Teste: `tst_web.cpp::interruptedDownloadsAreDistinguished` (Task 4).

---

### Task 1: Publicação atômica do backup

**Files:**
- Modify: `src/backup.cpp:84-96`
- Test: `tests/tst_backup.cpp`

**Interfaces:**
- Consumes: `zs::createBackup(profileDir, backupDir, now, outPath, error)` (já existente).
- Produces: mesma assinatura; nenhuma mudança de API.

- [ ] **Step 1: Escrever testes RED**

Em `tests/tst_backup.cpp`, declarar `existingArchiveIsReplacedWithoutGap()` e
`failedPublishKeepsPreviousArchive()` e implementar:

```cpp
void TestBackup::existingArchiveIsReplacedWithoutGap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));

    const QDateTime now(QDate(2026, 11, 23), QTime(21, 0, 0));
    const QString archive = backups + "/" + zs::backupName(now);

    // Primeiro backup: conteúdo antigo.
    QFile oldMarker(profile + "/marker.txt");
    QVERIFY(oldMarker.open(QIODevice::WriteOnly));
    oldMarker.write("antigo");
    oldMarker.close();
    QString first;
    QString error;
    QVERIFY2(zs::createBackup(profile, backups, now, &first, &error), qPrintable(error));
    QCOMPARE(first, archive);

    // Segundo backup no mesmo timestamp: precisa substituir, sem janela sem archive.
    QFile newMarker(profile + "/marker.txt");
    QVERIFY(newMarker.open(QIODevice::WriteOnly));
    newMarker.write("novo");
    newMarker.close();
    QString second;
    QVERIFY2(zs::createBackup(profile, backups, now, &second, &error), qPrintable(error));
    QCOMPARE(second, archive);
    QVERIFY(QFile::exists(archive));
    QVERIFY(!QFile::exists(archive + QStringLiteral(".partial")));

    // O archive publicado contém o conteúdo novo.
    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xzOf"), archive,
                                      QStringLiteral("profile/marker.txt")});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.readAllStandardOutput(), QByteArray("novo"));
}

void TestBackup::failedPublishKeepsPreviousArchive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));
    QVERIFY(QDir().mkpath(backups));

    QFile marker(profile + "/marker.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("sobrevivente");
    marker.close();

    const QDateTime now(QDate(2026, 11, 23), QTime(21, 5, 0));
    const QString archive = backups + "/" + zs::backupName(now);
    QString error;
    QString out;
    QVERIFY2(zs::createBackup(profile, backups, now, &out, &error), qPrintable(error));
    QVERIFY(QFile::exists(archive));

    // Destino bloqueado: um diretório no lugar do archive. Tanto remove()
    // quanto rename() falham; o erro deve ser reportado sem apagar o antigo.
    QVERIFY(QFile::remove(archive));
    QVERIFY(QDir().mkpath(archive));
    QString retryError;
    QVERIFY(!zs::createBackup(profile, backups, now, nullptr, &retryError));
    QVERIFY(QDir(archive).exists()); // archive anterior intacto
    QVERIFY(!retryError.isEmpty());
}
```

- [ ] **Step 2: Rodar o teste e registrar o baseline**

Run: `cmake --build build --target tst_backup --parallel && ctest --test-dir build -R tst_backup --output-on-failure`
Expected: ambos PASSAM no código atual — a janela `remove→rename` não é observável por teste
guiless determinístico (garantia POSIX de `rename()` documentada na spec). Registrar o resultado
como baseline; a mudança do Step 3 é validada por revisão do diff e pela suíte completa no Step 4.
Não inventar RED artificial.

- [ ] **Step 3: Implementar publicação atômica em `src/backup.cpp`**

Remover o bloco `if (QFile::exists(archive) && !QFile::remove(archive)) { ... }` (linhas 85-90).
Deixar apenas o `QFile::rename(partial, archive)` com tratamento de erro, precedido de comentário:

```cpp
    // rename() é atômico em POSIX dentro do mesmo filesystem: substitui o
    // archive anterior sem deixar o caminho vazio entre remove e rename.
```

Em caso de falha, continuar removendo `partial` e preenchendo `error`.

- [ ] **Step 4: Rodar testes e confirmar GREEN**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: 9/9 suítes passam; `tst_backup` com 16 testes.

- [ ] **Step 5: Commit**

```bash
git add src/backup.cpp tests/tst_backup.cpp
git commit -m "fix: publica backup com rename atômico sem remover archive anterior"
```

---

### Task 2: Travar aquisição do IPC com flock

**Files:**
- Modify: `src/instance.cpp:1-68`
- Modify: `src/instance.h:43-49`
- Test: `tests/tst_instance.cpp`

**Interfaces:**
- Consumes: `InstanceLock::acquire(QString*)`, `InstanceLock::~InstanceLock()`.
- Produces: mesmo `acquire()`; nenhuma assinatura pública muda. Novo membro privado
  `int m_lockFd = -1`; helper privado `bool lockAcquire(QString *error)` e `void lockRelease()`.

- [ ] **Step 1: Escrever teste RED**

Em `tests/tst_instance.cpp`, declarar `concurrentAcquireHasSingleOwner()` e implementar:

```cpp
void TestInstance::concurrentAcquireHasSingleOwner()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    constexpr int kRacers = 4;

    std::atomic<int> atGate{0};
    std::atomic<bool> go{false};
    std::array<bool, kRacers> won{};
    QList<QThread *> threads;

    for (int i = 0; i < kRacers; ++i) {
        auto *t = QThread::create([&, i] {
            zs::InstanceLock lock(socketPath);
            atGate.fetch_add(1);
            while (!go.load())
                QThread::yieldCurrentThread();
            QString error;
            won[i] = lock.acquire(&error);
            if (won[i])
                QThread::msleep(60); // mantém a posse enquanto os outros tentam
        });
        threads << t;
        t->start();
    }

    while (atGate.load() < kRacers)
        QThread::yieldCurrentThread();
    go.store(true);
    for (QThread *t : threads) {
        QVERIFY(t->wait(5000));
        delete t;
    }

    const int owners = std::count(won.begin(), won.end(), true);
    QCOMPARE(owners, 1);
}
```

Incluir `<atomic>`, `<array>`, `<algorithm>` e `<QThread>` no topo do arquivo de teste.

- [ ] **Step 2: Rodar o teste e confirmar RED**

Run: `cmake --build build --target tst_instance --parallel && ctest --test-dir build -R tst_instance --output-on-failure`
Expected: falha intermitente/constante com `owners` 0, 2, 3 ou 4 — a janela `lstat→listen`
permite mais de um dono. Se passar uma vez, repetir 5× (`ctest --repeat until-fail:5 -R tst_instance`).

- [ ] **Step 3: Implementar o lock em `src/instance.h` / `src/instance.cpp`**

Em `instance.h`: adicionar `#include <QString>` já existente; novo membro privado
`int m_lockFd = -1;` e declarações `bool lockAcquire(QString *error);` / `void lockRelease();`.

Em `instance.cpp`: incluir `<fcntl.h>` e `<sys/file.h>`. Implementar:

```cpp
bool InstanceLock::lockAcquire(QString *error)
{
    const QString lockPath = m_socketPath + QStringLiteral(".lock");
    m_lockFd = ::open(QFile::encodeName(lockPath).constData(),
                      O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (m_lockFd < 0) {
        if (error) *error = QStringLiteral("não foi possível abrir o lock: ") + lockPath;
        return false;
    }
    if (::flock(m_lockFd, LOCK_EX | LOCK_NB) != 0) {
        lockRelease();
        if (error) *error = QStringLiteral("inicialização concorrente: ") + lockPath;
        return false;
    }
    return true;
}

void InstanceLock::lockRelease()
{
    if (m_lockFd >= 0) {
        ::flock(m_lockFd, LOCK_UN);
        ::close(m_lockFd);
        m_lockFd = -1;
    }
}
```

Em `acquire()`: chamar `lockAcquire(error)` como primeira ação dentro de `if (!m_server)` e
devolver `false` se falhar. O restante da sequência (`probe` → `lstat` → `removeServer` →
`listen`) continua igual — agora serializada. Em `~InstanceLock()`, chamar `lockRelease()`
**depois** de fechar/remover o servidor. Não remover o arquivo `.lock` (evita nova race).

- [ ] **Step 4: Rodar testes e confirmar GREEN**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: 9/9 suítes; `tst_instance` com 8 testes. Repetir
`ctest --repeat until-fail:10 -R tst_instance` para estabilidade.

- [ ] **Step 5: Commit**

```bash
git add src/instance.cpp src/instance.h tests/tst_instance.cpp
git commit -m "fix: serializa aquisição do socket com flock para evitar dois donos"
```

---

### Task 3: Diretórios privados (0700)

**Files:**
- Modify: `src/paths.h`, `src/paths.cpp`
- Modify: `src/profile.cpp:34-35`
- Modify: `src/backup.cpp:69` e `:166`
- Test: `tests/tst_paths.cpp`

**Interfaces:**
- Consumes: nada de novo.
- Produces: `void zs::paths::ensurePrivateDir(const QString &path);` — cria o diretório se
  faltar e aplica modo `0700` sempre (inclusive em diretório já existente).
  Consumidores: `Profile::Profile()` chama para `profileDir()` e `cacheDir()`;
  `createBackup()` chama para `backupDir()`; `restoreBackup()` chama para `info.absolutePath()`.

- [ ] **Step 1: Escrever teste RED**

Em `tests/tst_paths.cpp`, declarar `existingDirectoryBecomesPrivate()` e implementar:

```cpp
void TestPaths::existingDirectoryBecomesPrivate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString target = dir.path() + "/zapshell/profile";
    QVERIFY(QDir().mkpath(target));
    QVERIFY(QFile::setPermissions(target,
            QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
            QFile::ReadGroup | QFile::ExeGroup |
            QFile::ReadOther | QFile::ExeOther));
    QFileInfo before(target);
    QVERIFY(before.permissions() & QFile::PermGroup);

    zs::paths::ensurePrivateDir(target);

    QFileInfo after(target);
    QCOMPARE(after.permissions(),
             QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    QFile probe(target + "/dados.txt");
    QVERIFY(probe.open(QIODevice::WriteOnly)); // conteúdo continua gravável
    probe.write("ok");
    probe.close();
}
```

Incluir `<QDir>`, `<QFile>`, `<QTemporaryDir>` e `<QFileInfo>` no teste.

- [ ] **Step 2: Rodar o teste e confirmar RED**

Run: `cmake --build build --target tst_paths --parallel && ctest --test-dir build -R tst_paths --output-on-failure`
Expected: compila mas falha em `after.permissions()` — `ensurePrivateDir` não existe ainda;
se não compilar, erro "`ensurePrivateDir` is not a member of `zs::paths`" também é RED válido.

- [ ] **Step 3: Implementar `ensurePrivateDir()` e ligar aos chamadores**

Em `paths.h`, declarar `void ensurePrivateDir(const QString &path);` com comentário
"cria e força modo 0700 (sessão, histórico e mídia são dados privados)".

Em `paths.cpp`, incluir `<QFile>` e implementar:

```cpp
void ensurePrivateDir(const QString &path)
{
    if (!QDir().mkpath(path))
        return;
    QFile::setPermissions(path,
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}
```

Substituir em `profile.cpp` as duas linhas `QDir().mkpath(...)` por
`paths::ensurePrivateDir(...)`. Em `backup.cpp`, substituir o `mkpath(backupDir)` do
`createBackup()` e o `mkpath(info.absolutePath())` do `restoreBackup()` por
`paths::ensurePrivateDir(...)`. `backup.cpp` já inclui `paths.h` — conferir; se não, incluir.

- [ ] **Step 4: Rodar testes e confirmar GREEN**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: 9/9 suítes; `tst_paths` com 4 testes.

- [ ] **Step 5: Commit**

```bash
git add src/paths.h src/paths.cpp src/profile.cpp src/backup.cpp tests/tst_paths.cpp
git commit -m "fix: força modo 0700 nos diretórios de perfil, cache e backup"
```

---

### Task 4: Notificação de download interrompido

**Files:**
- Modify: `src/web.h`, `src/web.cpp`
- Modify: `src/profile.h`, `src/profile.cpp`
- Modify: `src/app.cpp`
- Test: `tests/tst_web.cpp`

**Interfaces:**
- Consumes: `web::shouldNotifyDownload(bool finished, bool completed)` (já existente).
- Produces:
  - `bool zs::web::shouldNotifyInterrupted(bool finished, int state);`
    — `true` somente quando `finished` e `state` for `DownloadCancelled` ou `DownloadInterrupted`.
  - `Profile::downloadInterrupted(const QString &fileName)` — novo sinal.
  - Constantes `zs::DownloadCancelled` / `zs::DownloadInterrupted` em `webfeatures.h`,
    com `static_assert` em `web.cpp` contra `QWebEngineDownloadRequest::DownloadState`.

- [ ] **Step 1: Escrever teste RED**

Em `tests/tst_web.cpp`, declarar `interruptedDownloadsAreDistinguished()` e implementar:

```cpp
void TestWeb::interruptedDownloadsAreDistinguished()
{
    QVERIFY(!zs::web::shouldNotifyInterrupted(false, zs::DownloadCancelled));
    QVERIFY(zs::web::shouldNotifyInterrupted(true, zs::DownloadCancelled));
    QVERIFY(zs::web::shouldNotifyInterrupted(true, zs::DownloadInterrupted));
    QVERIFY(!zs::web::shouldNotifyInterrupted(true, 0)); // DownloadRequested
    QVERIFY(!zs::web::shouldNotifyInterrupted(true, 2)); // DownloadCompleted
}
```

Incluir `webfeatures.h` já presente no teste.

- [ ] **Step 2: Rodar o teste e confirmar RED**

Run: `cmake --build build --target tst_web --parallel && ctest --test-dir build -R tst_web --output-on-failure`
Expected: falha de compilação — `shouldNotifyInterrupted` e as constantes não existem.

- [ ] **Step 3: Implementar e ligar sinal**

Em `src/webfeatures.h`, adicionar constantes `DownloadCancelled = 3` e
`DownloadInterrupted = 4`, conferidas no header do Qt 6
(`/usr/include/qt6/QtWebEngineCore/qwebenginedownloadrequest.h:41-47`):
`DownloadRequested=0, DownloadInProgress=1, DownloadCompleted=2, DownloadCancelled=3,
DownloadInterrupted=4`.

Em `src/web.cpp`, adicionar `static_assert` para as duas constantes e implementar:

```cpp
bool shouldNotifyInterrupted(bool finished, int state)
{
    return finished && (state == DownloadCancelled || state == DownloadInterrupted);
}
```

Em `src/profile.h`, declarar o sinal `void downloadInterrupted(const QString &fileName);`.

Em `src/profile.cpp`, no lambda de `isFinishedChanged`: se `shouldNotifyDownload(...)` for
falso e `shouldNotifyInterrupted(download->isFinished(), int(download->state()))` for verdadeiro,
emitir `downloadInterrupted(download->downloadFileName())`.

Em `src/app.cpp`, adicionar:

```cpp
    connect(&profile, &Profile::downloadInterrupted, &notifier, [&notifier](const QString &fileName) {
        notifier.show(QStringLiteral("Download interrompido"), fileName);
    });
```

ao lado do `connect` de `downloadFinished` existente.

- [ ] **Step 4: Rodar testes e confirmar GREEN**

Run: `cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: 9/9 suítes; `tst_web` com 10 testes.

- [ ] **Step 5: Commit**

```bash
git add src/web.h src/web.cpp src/webfeatures.h src/profile.h src/profile.cpp src/app.cpp tests/tst_web.cpp
git commit -m "fix: notifica download cancelado ou interrompido separadamente"
```

---

### Task 5: Documentação e verificação final

**Files:**
- Modify: `README.md`
- Test: nenhum novo — valida o estado entregue.

**Interfaces:**
- Consumes: saídas das Tasks 1-4.
- Produces: README atualizado; nenhuma API nova.

- [ ] **Step 1: Atualizar README**

Na seção "Dados", registrar que os diretórios são criados com modo `0700` e que backups
são publicados por `rename()` atômico após validação. Na seção "Notificações e permissões",
registrar que downloads concluídos e interrompidos geram notificações distintas. Na seção
"Limitações honestas", manter memória/chamadas/X11 intactos; não prometer melhorias de
desempenho sem benchmark.

- [ ] **Step 2: Verificação completa local**

Run:
```bash
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```
Expected: `100% tests passed out of 9`.

Run: `makepkg -f` e conferir `100% tests passed` no `check()` e `namcap` sem erros.

- [ ] **Step 3: Evidência de permissões (ramo autorizado)**

Com o usuário tendo aprovado o `chmod`, conferir após a próxima inicialização:
```bash
stat -c '%a %n' "$HOME/.local/share/zapshell" "$HOME/.local/share/zapshell/profile" "$HOME/.cache/zapshell"
```
Expected: `700` nos três. **Não reiniciar o app sem o "pronto" do usuário.**

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: registra backup atômico, diretórios 0700 e notificação de download interrompido"
```

---

## Self-review (feito antes de salvar)

- **Cobertura da spec:** A0 → Task 1; A (flock) → Task 2; B (permissões) → Task 3; C (interrompido) → Task 4; D (README) → Task 5. Tipos especiais no restore ficaram como "parcial" na spec sem item de correção — coberto por validação textual existente, fora do escopo deste plano.
- **Tipos/assinaturas:** `shouldNotifyInterrupted(bool, int)` consistente entre Task 4 (teste, implementação, chamador). `ensurePrivateDir(const QString&)` consistente entre Tasks 3. Nenhuma outra task depende de nomes definidos em tasks posteriores.
- **Review Focus:** os cinco riscos listados têm teste em tarefa própria. Atomicidade POSIX em si não é testável sem hook; o teste cobre o resultado observável.
- **Proporção:** plano focado em decisões (arquivos, assinaturas, valores, comandos). Corpos de teste aparecem só onde as asserções fixam o comportamento.
