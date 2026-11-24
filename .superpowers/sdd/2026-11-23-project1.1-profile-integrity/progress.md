# SDD ledger — plan: docs/superpowers/plans/2026-11-24-project1.1-audit-followup.md

## Contexto
- Base: `origin/main` @ `01194c8f6209ccdeff13ef9e0addb77d106501da` (merge PR #1, Projeto 1.1).
- Branch corretiva: `fix/project1.1-audit-followup`.
- Worktree: `/home/alvaro/.local/share/opencode/worktree/53807d/project1.1-audit-followup`.
- Auditoria independente identificou ZS-10..ZS-15; plano corretivo aprovado pelo usuário.
- Restrições: sem paths.cpp/StatsSampler/D-Bus/notificações/flags Chromium/deps; sem perfil real; sem systemctl real; sem instalar; sem push/merge/publicação.

## Decisões definitivas (usuário)
- ZS-10: orçamento único 15s compartilhado waitUntilReleased+acquireMaintenance em backup/restore; QDeadlineTimer/monotônico; não inclui TAR; não reinicia; esgotado => abortar sem acessar perfil; tolerância 16.5s só no teste.
- ZS-11: conservadora; não relançar após qualquer falha de aquisição de manutenção, independente de stats; remover heurística de ausência de IPC; stderr claro + orientação de relançamento manual quando aplicável; preservar relaunch após falha de backup só quando manutenção adquirida e perfil íntegro/política original permitir.
- ZS-12: não liberar lock existente para readquirir; acquireMaintenance idempotente se já tem lock de manutenção; acquire idempotente se já tem lock de app; rejeitar transições manutenção<->app no mesmo objeto preservando descritor; teste de transição inválida não libera exclusividade.
- ZS-13: checar retorno de startDetached; stderr em falha; preservar resultado da manutenção; sem retry.
- ZS-14: reproduzir wasRunning=true + perda de lock; corrigir teste que exige relaunch após falha de aquisição; startup concorrente durante restore real que possui lock; sync explícita; quit/toggle com instância ativa; cleanup em falha de assert; rollback não confirmado: se não reproduzível, registrar limitação/parcial.
- ZS-15: fourProcessesSingleOwner com 4 processos independentes, barreira `go`, dono mantém lock até todos registrarem, exatamente 1 owner e 3 rejeitados, cleanup determinístico; sem threads.

## Preflight

A executar antes da Task 1.

| Par | Produz / Consome | Achado | Ruling |
|---|---|---|---|
| T1↔T2/T3/T4 | T1 altera API/semântica de `InstanceLock`; T2-T4 usam lock nos fluxos | Sem contradição direta; T1 deve preservar comportamento existente usado por fluxos. | Executar T1 primeiro; T2/T3 assumem API idempotente/rejeição de transição conforme spec. |
| T2↔T3 | Ambos mexem em `backupFlow`/`restoreFlow` (orçamento e relaunch) | Alto risco de conflito textual/semântico no mesmo bloco de falha de manutenção. | Executar em sequência: T2 (orçamento) antes de T3 (política relaunch). T3 deve preservar orçamento compartilhado de T2. |
| T3↔T4 | T3 altera caminhos de falha/relaunch; T4 adiciona checagem de `startDetached` | Mesma área de relaunch em `main.cpp`. | T4 após T3; aplicar check de retorno em todos os `startDetached` remanescentes. |
| T3↔T6 | T3 muda semântica de relaunch; T6 reescreve testes que dependem dela | Conflito esperado se fora de ordem. | T6 após T3; T6 adota política conservadora ZS-11 como oráculo. |
| T5↔T6 | Ambos tocam `tests/tst_instance.cpp` | Possível conflito de slots/helpers. | T5 antes de T6; T6 integra/ajusta sem remover cobertura de T5. |
| T1 auto | `acquire()` idempotente quando já tem lock de app - spec diz "quando o objeto já possui o lock da aplicação" | Precisa definir: server ativo = lock de app; lock fd sem server = manutenção. | Ruling: `acquire()` retorna true se `m_server!=nullptr`; se `m_lockFd>=0 && !m_server` rejeita (transição manutenção->app). `acquireMaintenance()` retorna true se `m_lockFd>=0 && !m_server`; se `m_server!=nullptr` rejeita (app->manutenção). Não liberar fd em rejeição. |
| T4 auto | startDetached failure hard to reproduce reliably | Spec pede verificar retorno; teste confiável pode ser difícil. | Tentar teste real via wrapper executável/injetável no PATH ou binário substituto; se não confiável, implementar check+stderr e registrar limitação com evidência no ledger (ZS-14 permite parcial só para rollback não confirmado, mas faremos esforço real). |
| Spec vs plano | Spec antiga dizia "startDetached falha => erro stderr"; plano antigo tinha relaunch preservado em falha de manutenção sem responder. | Conflito resolvido pelas decisões definitivas ZS-11/ZS-13 do usuário. | Decisões definitivas do usuário prevalecem; atualizar docs na Task 7. |

Scan concluído: sem bloqueadores. Conflitos tratados por ordenação e rulings acima.

## Task 1 — ZS-12
- Dispatch 1 (codex ses_df150fd7affeAzfhO1HmUHnFCm): BLOCKED por permissões (sem shell/write). Sem mudanças. Re-dispatch com agent `general`.
- Review flint (ses_df146d334ffeqI1OoOl8SEUj2U): **Needs fixes**. Important: `invalidTransitionMaintenanceToAcquirePreservesExclusivity` RED não discriminante (passou pre-fix por fd leak); falta asserção de re-check same-object `acquireMaintenance()` após rejeição. Minor: report "6 passed"/4 tests, diff indentação, stale error hygiene.
- Fix round 1: despachado ao implementador original (ses_df14e7850ffeYeJ5d4T1ZfKrAM).
- Fix round 1 (164ad70): ADRESSED, re-review (ses_df13a6c01ffe45aHmmlAPi0Epv) limpo. Minor: line mapping drift.
- Task 1: complete (commits 01194c8..164ad70, review clean after fix round 1). Base para Task 2: 164ad70.
- Task 2 (ZS-10): commit 3c009b0. Review flint falhou por rate limit (ses_df126d8cfffe1wkUmKPLnUpgY3); re-dispatch review para `general`.
- Task 2 review (ses_df122cca8ffeZ3yfWRrz41X4Cz): **Approved**. Important: falta teste simétrico de orçamento/contenção para restoreFlow; mensagens hardcodam "15000 ms" quando valor efetivo é remaining. Minor: report descreve relaunch em budget exhaustion mas código aborta conservador; `readBefore` não usado no teste. Carregar para Task 3/6.
- Task 2: complete (3c009b0). Base para Task 3: 3c009b0.
- Task 3 (ZS-11): commit df83b7a. Review em andamento (ses_df115ca8fffeUGGnvG7sRiSplX). Nota: full ctest --timeout 80 deu timeout em tst_instance (80.01s) mas tst_instance passou focado/standalone (~95s) — tratar como risco de validação antes do fechamento.
- Task 3 review (ses_df115ca8fffeUGGnvG7sRiSplX): **Needs fixes**. Critical: `backupWasRunningLockContentionDoesNotRelaunch` não cria `wasRunning=true` real. Important: `ctest --timeout 80` estourou em tst_instance (80.01s). Minor: cleanup/comentário morto.
- Fix round 1 despachado ao implementador original (ses_df11ee93cffee1X57yC128SrYY).
- Task 3 fix round 1 (a03191a): `backupWasRunningLockContentionDoesNotRelaunch` agora reproduz wasRunning real; waits reduzidos; `tst_instance` dividido em `tst_instance` + `tst_instance_contention` no CTest para cumprir gate de 80s (10/10 pass, 90.42s total). Re-review escopado em andamento.
- Task 3 re-review (ses_df0fc7211ffeHap47jWPaXAMmD): Finding 1/3 ADDRESSED; Finding 2 NOT ADDRESSED por evidência stale no report (report mostrava 9 testes/timeout). Controller reexecutou comando exigido após split CTest: 10/10 pass, 90.42s total, nenhum timeout por teste (`--timeout 80` é por teste). Atualizando report e despachando re-review de evidência.
- Task 3 re-review evidência (ses_df0fb2cddffe0FON1hH1LO3912): Finding 2 ADDRESSED. Comando exigido reexecutado após reconfiguração CTest: 10/10 pass, 90.42s total, sem timeout por teste. Minor: risco de manutenção por lista explícita de casos no CTest.
- Task 3: complete (df83b7a, a03191a). Base para Task 4: a03191a.
- Task 4 (ZS-13): commit 92d1af5. Review (ses_df0f0b9fbffeCTPpvfmP7qpYKl): **Approved**. Important: ramo de falha startDetached sem cobertura automatizada/hook mínimo (limitação documentada). Minor: stderr sem contexto do path. Risco aceito por escopo/viabilidade.
- Task 4: complete. Base para Task 5: 92d1af5.
- Task 5 (ZS-15): commit 23e9b3e. Review em andamento (ses_df0e60decffeaQ53Svzt9pqBXz).
- Task 5 review (ses_df0e60decffeaQ53Svzt9pqBXz): **Needs fixes**. Important: race publicação/leitura resultado filho (tmp+rename atômico). Minor: validar erro não-vazio em rejected. Fix round 1 despachado (ses_df0ef92caffeOg8BqrMbfx0Si8).
- Task 5 re-review (ses_df0e23657ffe3jMudE79cYEoiQ): findings ADDRESSED, sem new breakage. Task 5 complete (23e9b3e,74fad64). Base para Task 6: 74fad64.
- Task 6 review (ses_df06db592ffewTsxXSXl2obniz): **Needs fixes**. Important: restoreBlocksProfileStartup probe adquire lock e o segura 10ms quando restore não detém lock, interferindo no restore real; contenção condicional/qWarning. Fix: probe scoped com release imediato; quando lock observado, afirmar startup sem socket durante janela observada sem re-probing intrusivo. Important: restoreFlowSharedBudgetCapsWaitAndLockContention sem ThreadJoinGuard (std::terminate em assert). Minor: limitação qWarning.
- Fix round 1 despachado (ses_df0e190bfffezrxmHhGvZknzvP).
- Task 6 fix round 1 (333bac1): probe não intrusivo + ThreadJoinGuard. Re-review escopado em andamento (ses_df067bf7cffeBY0otPkgDEaqN1).
- Task 6 re-review round 1: Finding 1/3 ADDRESSED, Finding 2 NOT ADDRESSED (guard aplicado no teste errado). Fix 3241df4 aplica guard correto em restoreFlowSharedBudgetCapsWaitAndLockContention. Re-review round 2 em andamento (ses_df064d7e9ffeIZ7YoGoDXNyC78).
- Task 6 re-review round 2 (ses_df064d7e9ffeIZ7YoGoDXNyC78): Finding 2 ADDRESSED, sem new breakage. Task 6 complete (e6f191f,333bac1,3241df4). Base para Task 7: 3241df4.

## Task 7 — Documentação, validação final, empacotamento

- Status: complete (working tree, sem commits).
- Docs atualizados: `README.md`, `docs/superpowers/specs/2026-11-23-project1.1-profile-integrity-design.md`, `docs/superpowers/plans/2026-11-23-project1.1-profile-integrity.md`, este ledger.
- Plano follow-up presente: `docs/superpowers/plans/2026-11-24-project1.1-audit-followup.md`.
- Evidências: `build/ctest/makepkg/namcap/sha256` registradas em `.superpowers/sdd/2026-11-24-project1.1-audit-followup/task-7-report.md`.

## Resumo Tasks 1-7 (audit follow-up ZS-10..ZS-15)

| Task | ID | Status | Commits | Evidência principal |
|---|---|---|---|---|
| 1 | ZS-12 | complete | 4e1c7f1, 164ad70 | Lock idempotente/transições rejeitadas preservando fd |
| 2 | ZS-10 | complete | 3c009b0 | Orçamento único 15s compartilhado (QDeadlineTimer) |
| 3 | ZS-11 | complete | df83b7a, a03191a | Relaunch conservador; sem heurística de ausência de IPC |
| 4 | ZS-13 | complete | 92d1af5 | `startDetached()` checado, stderr, sem retry |
| 5 | ZS-15 | complete | 23e9b3e, 74fad64 | 4 processos independentes, barreira `go`, resultado atômico |
| 6 | ZS-14 | complete | e6f191f, 333bac1, 3241df4 | Contenção/relaunch/IPC ativo/cleanup; rollback não confirmado parcial |
| 7 | Docs/validação/pkg | complete | none | README/spec/plan/ledger + build/ctest/makepkg/namcap/sha256 |

## Status final

- ZS-10..ZS-13, ZS-15: resolvidos.
- ZS-14: resolvido com limitação documentada (rollback não confirmado parcialmente validado; observabilidade de restore sob carga).
- Pendências: validação real `systemctl --user`; sem install/push/merge/publicação.

## Encerramento Projeto 1.1 (HEAD d105425, 2026-11-24)

### Critérios de aceitação atendidos
- ZS-10: orçamento único 15s compartilhado (`waitUntilReleased` + `acquireMaintenance`) em backup/restore, `QDeadlineTimer` monotônico, sem incluir TAR, sem reset, esgotado aborta sem acessar perfil. Testes discriminantes A1 (mutação RED: backup 18283ms / restore 18144ms; GREEN 30488ms; limite <16500ms).
- ZS-11: política conservadora; nenhuma falha de aquisição de lock de manutenção dispara relaunch, independente de `stats`; heurística de ausência de IPC removida; stderr claro; relaunch preservado só com manutenção adquirida e perfil íntegro/política original.
- ZS-12: `acquire()`/`acquireMaintenance()` idempotentes no mesmo modo; transições manutenção<->app rejeitadas preservando descritor/exclusividade; sem liberar lock para readquirir.
- ZS-13: `startDetached()` checado em backup/restore; stderr em falha; resultado da manutenção preservado; sem retry. Cobertura automatizada do ramo de falha runtime: limitação aceita (inspeção estática + comportamento implementado).
- ZS-14: `wasRunning=true`+perda real de lock; startup concorrente durante restore real com lock comprovado por wrapper TAR determinístico/barreira (B1, sem QSKIP); quit/toggle com instância ativa; cleanup RAII. Mutação complementar ausência de lock detectada. Rollback não confirmado: limitação aceita, parcialmente validado por inspeção estática.
- ZS-15: `fourProcessesSingleOwner` com 4 processos independentes, barreira `go`, dono mantém lock até todos registrarem, exatamente 1 owner/3 rejeitados, cleanup determinístico, publicação tmp+rename atômica.
- Preservação dos testes anteriores após split CTest confirmada (11/11 pass, 108.55s, timeout 80s por teste).

### Riscos residuais aceitos
- ZS-13: sem indução determinística de falha de `startDetached`.
- ZS-14: rollback não confirmado sem teste runtime determinístico (política correta por inspeção estática).
- Atomicidade sem garantia de durabilidade (`rename` sem `fsync`).
- Integração/validação real `systemctl --user` pendente (não executada).
- Namcap: 0 erros, 4 warnings preexistentes (libgcc/glibc/libstdc++ implícitos; `qt6-webchannel` possivelmente desnecessário).

### Evidências preservadas
- RED/GREEN A1 mutante: `.superpowers/sdd/2026-11-24-project1.1-audit-followup/task-8-report.md`
- B1 determinístico + mutação complementar: `task-8-report.md`
- Revisões cegas/diffs/briefs: `.superpowers/sdd/2026-11-24-project1.1-audit-followup/`
- LastTest.log, makepkg, namcap, SHA256: evidência final de auditoria.

### Estado roadmap
- Projeto 1.1 (ZS-01/02/03/05 + ZS-10..ZS-15): **encerrado, aprovado com ressalvas registradas, aguardando publicação/merge autorizado**.
- Projeto 2: **não iniciado**; próximo passo após merge é diagnóstico e proposta de arquitetura de observabilidade (sem implementação).
