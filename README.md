<<<<<<< HEAD
# COOP-THEFT-AUTO-V
a coop mod made with ai and source code assistence
=======
# GTAV-Coop — Multiplayer Coop para o Modo História do GTA V

Transforma a campanha single-player em coop online: mundo compartilhado,
missões sincronizadas por estado, veículos com autoridade do motorista e
**save compartilhado versionado** (prioridade máxima do projeto).

> **Nota importante:** este projeto é 100% standalone. Não usa, deriva ou
> referencia a árvore de código-fonte vazada da Rockstar — isso seria
> ilegal e não-compilável. Todo o acesso ao jogo é via **natives do
> ScriptHookV** (padrão da comunidade de mods SP), com hashes gerados
> automaticamente do banco público de natives.

## Estrutura

```
GTAV-Coop/
├── shared/    código comum: log, CRC32, config, sockets, SaveStore (COOP1)
├── protocol/  protocolo de rede compartilhado cliente+servidor+testes
├── server/    servidor dedicado (sessão, relay, save autoritativo)
├── client/    plugin ASI (GTAVCoop.asi) para o GTA V
├── tests/     suíte de testes sem o jogo (protocolo + save)
├── tools/     gerador de natives (hashes exatos do banco público)
├── scripts/   build.ps1
└── docs/      arquitetura, protocolo, instalação, testes, limitações
```

## Começo rápido

```powershell
scripts\build.ps1 -Test          # compila tudo + roda testes
build\server\Release\coop_server.exe   # sobe o servidor
# copie build\client\Release\GTAVCoop.asi para a pasta do GTA V
# inicie o modo história, aperte F9, conecte no IP do servidor
```

Detalhes: [docs/INSTALL.md](docs/INSTALL.md)

## O que está implementado (v1)

- Conexão TCP+UDP com IP/porta, ping, heartbeat, timeout, reconexão
- Sessão 2–4+ jogadores, migração de host, tokens anti-spoof no UDP
- Representação de jogadores remotos (ped local interpolado, roupas, vida,
  armas, morte/respawn), lock de protagonista por jogador
- Veículos: motorista-autoridade, dano, pneus, luzes, spawn por GUID
- Mundo: relógio, clima, procurado, explosões
- Missões: FSM IDLE→STARTING→ACTIVE→CHECKPOINT→COMPLETED/FAILED com
  registro por missão; incompatíveis são declaradas (sem sync fake)
- Save cooperativo (modelo **Seamless Co-op**, só nos saves): formato COOP1
  (CRC32 por seção), escrita atômica, 5 backups rotativos, guard de versão,
  recuperação de corrupção, um save por grupo no servidor adotado
  automaticamente por quem entra, spawn na última posição do grupo, autosave
  configurável, push imediato no fim de missões, crash recovery automático
  via mirror local
- **Menu de skins**: F9 → Skins usa modelos de NPC como skin do jogador
  (polícia, serviços, civis, personagens da história) — sincronizado pra
  todos, incluindo quem entrar depois; "Original" restaura o protagonista
- Menu F9 discreto (conectar, jogadores, saves, configurações) + chat

## O que é honestamente limitado (v1)

Leia [docs/LIMITATIONS.md](docs/LIMITATIONS.md). Destaques: dinheiro SP
sem native estável (espelho informacional), NPCs de tráfego ambientais não
clonados, integração fina por missão exige registro missão-por-missão.

## Licença / uso

Uso privado e educacional. Não afiliado à Rockstar. Não use em GTA Online.
>>>>>>> f79c9ea (not tested)
