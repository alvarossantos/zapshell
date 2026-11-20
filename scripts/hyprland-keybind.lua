-- Fragmento para ~/.config/hypr/modules/binds.lua (Hyprland via Ryoku).
-- Colar depois das outras linhas `hl.bind(...)`, onde `mod` e `K` ja estao
-- definidos no escopo do arquivo.
--
-- Alterna a janela do zapshell: mostra/levanta quando oculto, esconde quando
-- visivel e ativo. Equivale ao `SUPER, W, exec, zapshell --toggle` da sintaxe
-- .conf, mas usa a sintaxe Lua adotada nesta maquina.
hl.bind(K(mod .. " + W"), function() hl.exec_cmd("zapshell --toggle") end)
