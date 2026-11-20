# zapshell — ícone flat colorido

Small change. Escolha feita na conversa: opção C ("Terminal + zap"), vista nos mockups.

## Intent / why
O app já está instalado com `Icon=zapshell`, mas o SVG atual é um balão verde genérico com "Z". Queremos arte própria, reconhecível na dock e no launcher, que comunique "shell + zap" sem copiar o WhatsApp oficial.

## Goals
- Um SVG vetorial em `assets/zapshell.svg`, instalado em `hicolor/scalable/apps`.
- Legível em 32, 48 e 128 px, sobre fundo claro e escuro.
- Paleta flat: fundo `#16232B`, balão `#25D366`, prompt `#FFFFFF`, raio `#FFD54F`.

## Non-goals
- Keybinds, daemon do Ryoku, `.desktop`, PKGBUILD ou comportamento do app.
- Gerar bitmaps PNG — o caminho `scalable` já cobre os tamanhos.

## Anti-goals
- Parecer demais com o WhatsApp oficial e confundir na dock.
- Detalhes finos que desapareçam em 32 px.

## Constraints
- Ícone resolvido pelo nome `zapshell` via XDG icon theme.
- Mudanças no desktop usam comandos do Ryoku; nunca editar arquivos shipados nem `binds.lua`.
- Sem `sudo`, `pacman` ou `systemctl` sem aprovação explícita.

## Approaches
| Opção | Desenho | Trade-off |
|---|---|---|
| A | Balão branco com "Z" + raio | Perto do WhatsApp; "Z" some em 32 px |
| B | Dois balões sobrepostos + `>_` | Comunica zap+shell, mas polui em 32 px |
| C | Fundo escuro, balão verde, `>_` branco + raio | **Escolhida**: distinta, alto contraste, `>_` legível em 32 px |

## Decisões já feitas
- Formato SVG: é o que o pacote já instala e escala sem perda.
- Opção C, mantido o raio amarelo.
- `viewBox` 64×64, cantos arredondados `rx=16`.

## Deixado ao builder
- Ajustar coordenadas finais para `>_` e raio ficarem alinhados também em 32 px.
- Validar render nos 3 tamanhos antes de instalar.
- Instalar no tema via comando do Ryoku/ XDG, não por escrita direta em `/usr`.
