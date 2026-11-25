# Arquitetura — GTAV-Coop

## Visão geral

```
+---------------------+        +----------------------+       +----------------+
|  GTA V (host)       |        |  Servidor dedicado   |       | GTA V (cliente)|
|  GTAVCoop.asi       |<------>|  coop_server.exe     |<----->| GTAVCoop.asi   |
|  - ClientNet        | TCP    |  - sessão + relay    | TCP   | - ClientNet    |
|  - Managers         | UDP    |  - save autoritativo | UDP   | - Managers     |
+---------------------+        +----------------------+       +----------------+
```

- **Transporte**: TCP (controle, saves, missões, chat) + UDP (movimento,
  veículos, mundo). Formato de frame: `[u8 ver][u8 msgId][u16 len][payload]`.
- **Autoridade**: host (primeiro cliente) comanda missões, spawns e mundo;
  servidor comanda sessão e save. Migração de host automática.
- **Motor do jogo**: acessado exclusivamente via natives do ScriptHookV
  (camada `ShvCompat`, carregamento dinâmico — sem .lib do SDK).

## Sistemas (conforme spec)

| Manager | Arquivo | Responsabilidade |
|---|---|---|
| NetworkManager | `client/src/net/ClientNet.*` | Conexão, filas thread-safe, ping, reconexão |
| PlayerManager | `client/src/managers/PlayerManager.*` | Roster + captura local + interpolação de remotos |
| EntityManager | `client/src/managers/EntityRegistry.h` | GUID persistente ↔ handle local (anti-duplicação) |
| VehicleManager | `client/src/managers/VehicleSync.*` | Motorista-autoridade, dano, pneus, spawn |
| WorldSyncManager | `client/src/managers/WorldSync.*` | Relógio, clima, procurado, explosões |
| MissionSyncManager | `client/src/managers/MissionSync.*` | FSM IDLE→…→COMPLETED/FAILED, registro de missões |
| SaveManager | `client/src/managers/SaveManager.*` | Pull/apply do jogo, autosave, merge de saves |
| SessionManager | `server/src/ServerSession.*` | Sessão, relay, save host, timeouts, kick |

## Save compartilhado

Formato `COOP1` (`shared/coop/save/SaveStore.*`): seções com CRC32, cabeçalho
com checksum, escrita atômica (temp+rename), 5 backups rotativos, guard de
versão (nunca sobrescreve versão mais nova), recuperação por backup em caso
de corrupção. Os saves nativos do jogo **nunca** são lidos ou escritos.

### Modelo "Seamless" (inspirado no Seamless Co-op de Elden Ring)

O save é do **grupo**, não de pessoa — ninguém gerencia nada:

- **Save por grupo no servidor** (`saves/<groupId>.coop`): todo mundo que
  conecta no mesmo servidor compartilha a mesma campanha automaticamente.
- **Adoção automática ao entrar**: o WELCOME entrega o blob do save
  compartilhado; o cliente aplica (armas, vida, progresso) sem menu nem
  "carregar jogo".
- **Join na posição do grupo**: cada pull do host grava a última posição no
  save; quem entra spawna ali (sem menu de spawn).
- **Autosave + push em fim de missão**: a cada 2 min (configurável) e no
  instante em que uma missão do host termina (COMPLETED ou FAILED), o save
  do grupo é atualizado no servidor — fechar o jogo nunca perde progresso.
- **Crash recovery automático**: se o servidor perder o save mas um jogador
  tiver mirror local mais novo (crash no meio de uma escrita), ele mantém o
  próprio estado e o empurra ao servidor na hora da adoção — o grupo segue
  do ponto mais recente sem intervenção.

## Threading

- **Script thread (SHV)**: únicos que tocam natives e estado do jogo
- **Network thread**: sockets + filas (mutex), nunca toca o jogo
- **SHV keyboard hook**: só alterna estado do menu (thread-safe via flag)

## Natives

`tools/gen_natives.ps1` gera `NativeHashes.h` + `NativeWrappers.h` a partir
do banco público de natives (alloc8or). Assinaturas exatas do banco — nada
de hashes ou parâmetros "de memória". Semântica de `nativeCall` verificada
contra o SDK público (MenyooSP `nativeCaller.h`).
