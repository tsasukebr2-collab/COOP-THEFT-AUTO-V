# Instalação e uso

## Requisitos

- GTA V (Legacy) para PC, modo história
- [ScriptHookV](http://www.dev-c.com/gtav/scripthookv/) instalado na pasta do jogo
- Visual Studio 2022 (Build Tools com C++) + CMake 3.20+ para compilar

## Compilar

```powershell
cd GTAV-Coop
scripts\build.ps1          # build
scripts\build.ps1 -Test    # build + roda a suíte de testes
```

Artefatos em `build\`:
- `server\Release\coop_server.exe`
- `client\Release\GTAVCoop.asi`
- `tests\Release\coop_tests.exe`

## Servidor

1. Copie `coop_server.exe` para uma pasta
2. (Opcional) crie `coop_server.ini`:

```ini
tcpPort = 31999
udpPort = 31998
maxPlayers = 4
groupId = default      # um save compartilhado por grupo: saves/<groupId>.coop
savePath =             # vazio = saves/<groupId>.coop (configure só p/ custom)
password =
autosaveIntervalMs = 120000
clientTimeoutMs = 15000
logLevel = 1
```

3. Rode `coop_server.exe`. O save compartilhado do grupo fica em `saves\` —
   **backupe essa pasta**; ela é a campanha do grupo. Para campanhas
   paralelas no mesmo servidor, mude `groupId` (ex.: `grupo-a`,
   `grupo-b`): cada grupo tem seu próprio save e progresso.

## Cliente (jogadores)

1. Copie `GTAVCoop.asi` para a pasta do GTA V (mesma pasta do
   `ScriptHookV.dll`)
2. Entre no modo história, carregue um jogo
3. Aperte **F9** → "Connect to server" → IP/porta/nome → Enter no
   "Quick connect" equivalente... (use a tela CONNECT: LEFT/RIGHT escolhe o
   campo, UP/DOWN edita, ENTER conecta)
4. **Skins**: F9 → "Skins (NPC models)" → UP/DOWN escolhe entre ~30 modelos
   de NPC (polícia, serviços, civis, personagens da história) → ENTER aplica.
   "Original" volta ao protagonista. Todos veem sua skin (sincronizada via
   servidor), e quem entra depois também.
5. O chip "CO-OP ping" no canto confirma a sessão

Para jogar em LAN/entre amigos: anfitrião roda o servidor na própria
máquina (`127.0.0.1`) ou alguém roda num PC extra; abra as portas
31999/TCP + 31998/UDP no roteador para jogar pela internet.

## Fluxo de sessão

1. Host conecta primeiro → torna-se host (autoridade de missões/mundo/save)
2. Clientes conectam → recebem WELCOME com o save compartilhado atual
3. Missões: host inicia normalmente; estado replica para os clientes
4. Autosave a cada 2 min (configurável) + push no fim de missões
5. Reconexão: basta conectar de novo — o save compartilhado restaura o
   progresso

## Saves seamless (estilo Seamless Co-op)

O save é **do grupo**: quem entra na sessão herda automaticamente a campanha
compartilhada do servidor (`saves/<groupId>.coop`), spawna na última posição
registrada do grupo e o progresso é salvo sem nenhuma ação do jogador —
autosave periódico, push imediato no fim de missões e recuperação automática
se o servidor perder o save (o mirror local mais novo vence e é re-empurrado).
Nada de "carregar save", transferir arquivos ou menus de save.

## Avisos

- Modo história apenas. O plugin não registra scripts quando o ScriptHookV
  não está ativo; não use em GTA Online
- Mantenha backup da pasta `saves\` do servidor
- Sem anti-cheat bypass: isso é para SP/privado
