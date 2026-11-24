# Task 8 Report — Pendências A1/B1 (auditoria final independente)

## Status
Completed (testes + evidência + validação). Commits locais isolados. Sem alterações em `src/` de produção. Sem install/push/merge/publish/systemctl real/sudo.

## Commits
- `d02e7f1` `test: torna ZS-10 discriminante para orçamento compartilhado`
- `547fe83` `test: torna restore concorrente ZS-14 determinístico`

Base auditada: `59198a6`. Diff `59198a6..547fe83` toca apenas `tests/tst_instance.cpp`.

## A1 — ZS-10 testes discriminantes

### Alterações
- `backupFlowSharedBudgetCapsWaitAndLockContention`: socket vivo responde `backup`; espera `~3000ms` com socket presente; fecha socket; mantém contenção de lock de manutenção; orçamento total `<16500ms`; demais asserções/RAII preservados.
- `restoreFlowSharedBudgetCapsWaitAndLockContention`: placeholder regular em `socketPath` (não-socket) faz `waitUntilReleased` consumir `~3000ms` enquanto `send("stats")` falha; placeholder removido; contenção de lock mantida; orçamento total `<16500ms`; demais asserções/RAII preservados.

### RED (mutação isolada, cópia descartável `/tmp/opencode/zs-a1-mutant`, base 59198a6 + testes corrigidos)
Mutação: reintroduzidos dois orçamentos independentes de 15s em `backupFlow` e `restoreFlow` (wait 15s + acquireMaintenance 15s). Código de produção da branch **não alterado**.

Comando:
```bash
XDG_DATA_HOME=/tmp/zs-audit-xdg-data XDG_CACHE_HOME=/tmp/zs-audit-xdg-cache XDG_RUNTIME_DIR=/tmp/zs-audit-xdg-runtime QT_QPA_PLATFORM=offscreen \
  timeout 120 build/tests/tst_instance backupFlowSharedBudgetCapsWaitAndLockContention restoreFlowSharedBudgetCapsWaitAndLockContention
```

Resultado RED (ambos falharam pela asserção de tempo, não por erro de ambiente/IPC/timeout externo):
- `backupFlowSharedBudgetCapsWaitAndLockContention`: FAIL `elapsed < 16500` — **18283 ms** (`tests/tst_instance.cpp:895` no mutante)
- `restoreFlowSharedBudgetCapsWaitAndLockContention`: FAIL `elapsed < 16500` — **18144 ms** (`tests/tst_instance.cpp:986` no mutante)
- Totais: `2 passed, 2 failed, 0 skipped, 0 blacklisted, 36465ms`

### GREEN (HEAD corrigido, binário real)
Mesmo comando, build atual:
- `backupFlowSharedBudgetCapsWaitAndLockContention`: PASS
- `restoreFlowSharedBudgetCapsWaitAndLockContention`: PASS
- Totais: `4 passed, 0 failed, 0 skipped, 0 blacklisted, 30488ms`

## B1 — Restore concorrente determinístico

### Alterações
- `restoreBlocksProfileStartup` reescrito para usar wrapper `tar` em PATH isolado apenas no processo restore.
- Wrapper resolve `tar` real via `QStandardPaths::findExecutable("tar")`; exec para real em casos não-extração; em extração (`-x*`) escreve `extract-started`, aguarda `extract-release` com timeout de segurança, depois `exec` tar real. Sem recursão (PATH isolado + caminho absoluto real).
- Archive pequeno/válido; barreira fornece duração.
- Teste confirma: marcador `extract-started`; `acquireMaintenance(...,0)` falha durante extração (lock mantido); startup concorrente iniciado e vivo; sem socket ativo durante contenção; barreira liberada; restore `waitForFinished` normal, `exitCode==0`; conteúdo restaurado (`keep.txt == "original"`); startup adquire socket após liberação; cleanup RAII (ChildProcess, BarrierGuard libera barreira, quit/kill).
- Sem `QSKIP` por timeout/falha da operação principal. Asserções centrais não condicionais.

### Mutação isolada complementar (cópia descartável `/tmp/opencode/zs-b1-mutant`)
Mutação: `restoreFlow` sem wait/lock, executa restore direto (remove exclusão mútua).
Resultado: FAIL `!probe.acquireMaintenance(&probeErr, 0)` — "restore deve manter lock de manutenção durante extração controlada" (`tests/tst_instance.cpp:490` no mutante), **172ms**, sem timeout/env. Detecta ausência de exclusão mútua.

### GREEN (HEAD corrigido)
- `restoreBlocksProfileStartup`: PASS, `1664ms` (execução isolada).

## Estabilidade (3 execuções consecutivas dos 3 testes-alvo)

Comando: `build/tests/tst_instance backupFlowSharedBudgetCapsWaitAndLockContention restoreFlowSharedBudgetCapsWaitAndLockContention restoreBlocksProfileStartup`
- Run 1: `5 passed, 0 failed, 0 skipped, 32161ms`
- Run 2: `5 passed, 0 failed, 0 skipped, 32065ms`
- Run 3: `5 passed, 0 failed, 0 skipped, 31913ms`

## Validação completa

- Build: `cmake --build build --parallel` OK.
- CTest completo: `XDG_DATA_HOME=/tmp/zs-audit-xdg-data XDG_CACHE_HOME=/tmp/zs-audit-xdg-cache XDG_RUNTIME_DIR=/tmp/zs-audit-xdg-runtime QT_QPA_PLATFORM=offscreen ctest --test-dir build --timeout 80 --output-on-failure`
  - **11/11 passed, 0 failed, 108.55s**. Sem SKIP nos cenários obrigatórios.
- `59198a6..HEAD` diff sem arquivos em `src/`.

## Empacotamento

- `makepkg -f`: OK (`zapshell 0.1.0-1`, build concluída)
- `namcap`: 0 erros, 4 warnings (libgcc/glibc/libstdc++ implicitamente satisfeitas; `qt6-webchannel` possivelmente desnecessário — preexistentes)
- SHA-256 pacote: `ac708807e2f3066ad5b27c343468c803ebfa1d482a1c573c578061f76af4304e  zapshell-0.1.0-1-x86_64.pkg.tar.zst`

## Limitações aceitas (inalteradas)
- ZS-13 sem indução determinística de falha de startDetached.
- Rollback não confirmado validado parcialmente por inspeção estática.
- Atomicidade sem garantia de durabilidade.
- Systemd real pendente.
- Warnings namcap preexistentes.

## Restrições respeitadas
Sem alterações em produção/deps/perfil real/sudo/install/systemctl real/push/merge/publicação. Escopo restrito a A1/B1.
