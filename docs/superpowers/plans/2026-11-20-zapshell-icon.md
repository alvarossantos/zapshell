# Ícone zapshell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Substituir o SVG genérico do zapshell pela arte flat colorida aprovada (opção C) e fazê-la resolver na dock e no launcher sem `sudo`.

**Architecture:** Um único SVG vetorial em `assets/zapshell.svg` é a fonte da verdade. Um teste CTest valida conteúdo e renderização; a instalação usa `~/.local/share/icons/hicolor/scalable/apps/` (precedência sobre `/usr/share` via XDG), com atualização do cache do tema. Nada no código C++, no `.desktop` ou no PKGBUILD muda.

**Tech Stack:** SVG 1.1, Python 3 stdlib (`xml.etree.ElementTree`), `rsvg-convert`, CTest, GTK 3 GI (`Gtk.IconTheme`)

**Spec:** `docs/superpowers/specs/2026-11-20-zapshell-icon-design.md`

## Global Constraints

- `viewBox="0 0 64 64"`, cantos arredondados `rx="16"`.
- Paleta exata: fundo `#16232B`, balão `#25D366`, prompt `#FFFFFF`, raio `#FFD54F`.
- Instalar em `hicolor/scalable/apps` — **nenhum PNG**.
- Sem `sudo`, `pacman` ou `systemctl` sem aprovação explícita.
- Não tocar `~/.config/hypr/modules/binds.lua`, `.desktop`, PKGBUILD nem código C++.
- Scripts com `#!/usr/bin/env bash` + `set -euo pipefail` (shell interativo do usuário é fish 3.6+).

## Review Focus

- Arte com `<text>` quebra em 32 px e depende de fonte instalada → Task 1 Step 1: `assert root.findall('.//{*}text') == []`.
- SVG malformado derruba o launcher inteiro → Task 1 Step 1: `ET.parse()`; Step 4: `rsvg-convert` nos 3 tamanhos.
- SVG antigo em `/usr/share` continua vencendo na resolução → Task 2 Step 3: `Gtk.IconTheme.lookup_icon()` deve apontar para `~/.local/share`.
- Cache `hicolor/icon-theme.cache` desatualizado esconde o ícone novo → Task 2 Step 2: `gtk-update-icon-cache -f -t`.
- Ícone em si legível em fundo escuro mas invisível em claro (ou vice-versa) → Task 1 Step 4: render 32/48/128 + inspeção visual nos dois fundos.

## File Structure

| Arquivo | Ação | Responsabilidade |
|---|---|---|
| `assets/zapshell.svg` | Modify | Única fonte da arte |
| `tests/tst_icon.sh` | Create | Valida XML, cores, ausência de fonte e render |
| `tests/CMakeLists.txt` | Modify | Registra `tst_icon` no CTest |
| `~/.local/share/icons/hicolor/scalable/apps/zapshell.svg` | Create (fora do repo) | Cópia instalada, precedência sobre `/usr/share` |

---

### Task 1: Arte SVG final + validação automatizada

**Files:**
- Modify: `assets/zapshell.svg`
- Create: `tests/tst_icon.sh`
- Modify: `tests/CMakeLists.txt` (após linha 18)

**Interfaces:**
- Consumes: opção C do spec (paleta, `viewBox`, `rx`).
- Produces: `assets/zapshell.svg` válido; teste `tst_icon` registrado no CTest, usado pela Task 2 como fonte da cópia instalada.

- [ ] **Step 1: Escrever o teste que falha**

Crie `tests/tst_icon.sh` executável (`chmod +x`), com `#!/usr/bin/env bash`, `set -euo pipefail` e duas verificações:

1. Python embutido (`python3 - "$SVG"`) com `xml.etree.ElementTree`:
   - `root.tag.endswith('svg')`
   - `root.get('viewBox') == '0 0 64 64'`
   - `root.find('{*}rect').get('rx') == '16'`
   - conjunto de atributos `fill` **e** `stroke` de todos os elementos ⊇ `{#16232B, #25D366, #FFFFFF, #FFD54F}` (normalize para minúsculas; `#FFFFFF` vem do `stroke` do `>_`)
   - `root.findall('.//{*}text') == []`
   - `len(root.findall('.//{*}path')) >= 4`
2. Loop `for size in 32 48 128`: `rsvg-convert -w "$size" -h "$size" "$SVG" -o "$tmp/$size.png"` e `test -s "$tmp/$size.png"`, com `mktemp -d` + `trap` para limpar.

`$SVG` deve resolver para `assets/zapshell.svg` na raiz do repo via `dirname "$0"/..`.

- [ ] **Step 2: Registrar no CTest**

Em `tests/CMakeLists.txt`, após `zs_test(tst_stats)`, adicione:

```cmake
add_test(NAME tst_icon COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tst_icon.sh)
```

Sem `zs_test()` — ela assume um `.cpp`.

- [ ] **Step 3: Rodar o teste e confirmar o FAIL**

Run: `ctest --test-dir build -R tst_icon --output-on-failure`
Expected: FAIL — SVG atual tem `rx="14"`, `#128c7e` e só 2 paths.

- [ ] **Step 4: Implementar a arte aprovada em `assets/zapshell.svg`**

Substitua o conteúdo por (arte C dos mockups, autocontida):

```svg
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" rx="16" fill="#16232B"/>
  <path d="M12 46l4.5-11A19 19 0 1 1 26 44z" fill="#25D366"/>
  <path d="M23 26l6 5-6 5" fill="none" stroke="#FFFFFF" stroke-width="4.2" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M32 36h8" fill="none" stroke="#FFFFFF" stroke-width="4.2" stroke-linecap="round"/>
  <path d="M48 8l-8 11.5h6L43 30l10-13.5H47z" fill="#FFD54F"/>
</svg>
```

5 paths (balão, 2 do `>_`, raio) — `>= 4` atendido. Nenhum `<text>`.

- [ ] **Step 5: Rodar o teste e conferir o PASS**

Run: `ctest --test-dir build -R tst_icon --output-on-failure`
Expected: PASS (6 asserts + 3 PNGs gerados).

- [ ] **Step 6: Inspecionar o render nos 3 tamanhos**

Abra os PNGs de 32/48/128 gerados em fundo claro e escuro. Se `>_` ou o raio colarem, ajuste só as coordenadas dos paths no SVG e repita os Steps 5–6.

- [ ] **Step 7: Rodar a suíte e commitar**

Run: `ctest --test-dir build --output-on-failure` → 8/8 PASS.
Commit: `git add assets/zapshell.svg tests/tst_icon.sh tests/CMakeLists.txt` + `git commit -m "feat: ícone flat colorido do zapshell"`.

---

### Task 2: Instalação no tema local + verificação de resolução

**Files:**
- Create (fora do repo): `~/.local/share/icons/hicolor/scalable/apps/zapshell.svg`

**Interfaces:**
- Consumes: `assets/zapshell.svg` da Task 1.
- Produces: `zapshell` resolvendo para a cópia da home no `Gtk.IconTheme`; dock e launcher passam a exibir a arte nova.

- [ ] **Step 1: Instalar a cópia na home**

Run: `install -Dm644 assets/zapshell.svg ~/.local/share/icons/hicolor/scalable/apps/zapshell.svg`
Expected: arquivo criado com modo `644`; nenhum `sudo`.

- [ ] **Step 2: Atualizar o cache do tema**

Run: `gtk-update-icon-cache -f -t ~/.local/share/icons/hicolor`
Expected: `gtk-update-icon-cache: Cache file created successfully.`

- [ ] **Step 3: Verificar a resolução**

Run (Python 3 com GI):

```python
import os
from gi.repository import Gtk
info = Gtk.IconTheme.get_default().lookup_icon('zapshell', 48, 0)
assert info is not None
assert info.get_filename().startswith(os.path.expanduser('~/.local/share/icons/')), info.get_filename()
print(info.get_filename())
```

Expected: caminho da home, não `/usr/share/...`.

- [ ] **Step 4: Verificar na dock e no launcher**

Abra o launcher com Super+Space e confirme a arte nova; repita na dock. Se o shell ainda mostrar o ícone antigo, abra e feche o launcher uma vez (o daemon do `ryoku-shell` está fora, então `ryoku reload` não é confiável nesta máquina).

- [ ] **Step 5: Confirmar o repo intacto**

Run: `git status --short` → sem mudanças novas (a instalação vive fora do repo). **Sem commit nesta tarefa.**
