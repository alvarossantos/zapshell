# Orçamento medido — zapshell

Medições em 19/11/2026, CachyOS/Arch, Qt 6.12.0 + qt6-webengine 6.140 (Chromium 140),
sessão Wayland. Soma de `Rss` e `Pss` de `smaps_rollup` de `zapshell` + todos os
descendentes (`/proc/<pid>/task/*/children`). Valores via `zapshell --stats` (RSS) e
script de leitura direta do `/proc` (PSS).

## Perfil de teste

Perfil **fresco e não pareado** (`XDG_DATA_HOME` temporário), na tela de QR do
WhatsApp Web. Não foi usado o perfil pareado do spike para não correr risco de
invalidar a sessão do celular do usuário. Números com perfil pareado e conversas
podem diferir — **falta medir nessa condição**.

| Cenário | RSS | PSS | CPU oculto |
|---|---:|---:|---:|
| Sem flags aplicadas (antes de ligar `qputenv`) | 937 MB | 669 MB | 0 % |
| `--disable-gpu` + heap 256 MB (amostra baixa) | 634 MB | 401 MB | 0 % |
| `--disable-gpu` + heap 256 MB (amostra alta, 40–60 s) | 976 MB | 710 MB | 0 % |
| + `IsolateOrigins,site-per-process` desligados | 968 MB | 700 MB | 0–4 % |

Variação de ~340 MB entre amostras do mesmo binário, com o mesmo perfil, causada
principalmente por um segundo processo renderer que o Chromium cria e libera ao
longo do carregamento do WhatsApp Web.

## Desvio

Metas do spec: oculto ≤ 320 MB, visível ≤ 350 MB, pico em vídeo ≤ 1.1 GB,
startup quente ≤ 2.5 s, CPU oculto ≤ 2 %, instalado ≤ 5 MB.

- **Não atingido:** RSS oculto (634–976 MB contra 320 MB). Mesmo PSS (401–710 MB)
  passa da meta.
- **Não atingido de forma estável:** CPU oculto (0–4 %).
- **Não medido:** pico em vídeo, startup quente, tamanho instalado.

## Flags agora padrão

`--renderer-process-limit=1`, `--js-flags=--max-old-space-size=256`,
`--disable-gpu`, `--disable-features=Translate,MediaRouter,OptimizationHints`,
`--disable-background-networking`.

`--disable-gpu` entrou como padrão porque a medição abaixou o pico, mas **ainda
não foi validado com ligação real** — se a chamada falhar com ele, a primeira
coisa a testar é removê-lo (`ZAPSHELL_FLAGS` substitui o conjunto inteiro).

`--no-zygote` foi descartado: o Chromium exige `--no-sandbox` junto, e isso
enfraquece a proteção do navegador sem garantia de ganho.

## Próximo passo

Medir com o perfil pareado e 3 conversas abertas. Se o desvio persistir, decidir
com o usuário entre aceitar o consumo, trocar a base tecnológica ou relaxar a
meta do spec.
