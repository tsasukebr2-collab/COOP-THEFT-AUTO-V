# Limitações conhecidas (honestas) e roadmap

Este documento lista o que NÃO está sincronizado na v1 e o motivo técnico.
Nada aqui é "fingido": onde não há sync, o sistema declara e trata.

## 1. Missões originais

| Cenário | Estado v1 |
|---|---|
| Estado da missão (IDLE→STARTING→ACTIVE→CHECKPOINT→COMPLETED/FAILED) | Sincronizado via `MissionSyncManager` (host = autoridade) |
| Início/falha/conclusão compartilhados | Sincronizado |
| Checkpoints | Sincronizados quando definidos no registro de missões (`maxCheckpoints`) |
| Detecção automática de conclusão por missão | Heurística (flag de missão + config). Cada missão com lógica própria precisa de entrada no registro |
| Missões unsupported (Prologue, escritório solo, Chop) | Declaradas no registro como `supported=false`: host joga solo, clientes são avisados e ficam de stand-down. Não há sync fake |
| Atores scriptados dentro de missões | Não sincronizados na v1 (requer integração por missão) |

Por quê: o fluxo interno de cada missão roda em bytecode scriptado
cliente-side; sem replicar o thread de script inteiro (escopo de engine), a
fonte de verdade correta é o host + um registro por missão. É o mesmo modelo
usado por projetos coop existentes.

## 2. Dinheiro

A carteira do SP não tem native estável (mora em globals version-dependent —
quebram a cada patch e corrompem se endereçados errado). A v1:

- Guarda espelho de dinheiro no save coop (informacional, UI)
- NÃO escreve no wallet do jogo (evita corrupção)
- Roadmap: tabela de offsets por versão do jogo, aplicada via `getGlobalPtr`
  com validação de versão

## 3. NPCs ambientais

- NPCs "compartilhados" (spawnados pelo sistema de spawn coop e atores de
  missão marcados) sincronizam posição/vida/morte (`MSG_NPC` no protocolo,
  `NpcSyncManager` ponto de encaixe)
- NPCs de tráfego ambientais NÃO são clonados entre máquinas na v1: cada
  cliente roda a população local do jogo; sincronizar tudo explode
  bandwidth e duplicaria entidades (o jogo já os spawna dos mesmos seeds na
  maioria dos casos)
- Polícia: nível de procurado e eventos de explosão são compartilhados

## 4. Física de veículos

- Motorista = autoridade; remotos aplicam estado com aproximação suavizada
  e snap em saltos > 15 m
- v1 usa lerp de posição/heading + sync de vida de motor/carroceria/pneus;
  integração de velocidade e blending de quaternion (evitar "sobe-e-desce"
  em rampas) são Fase 3

## 5. Reconexão

- Reconectar re-entra na sessão com o save compartilhado (continuidade de
  progresso garantida pelo SaveStore versionado)
- Reaproveitar o MESMO slot/clientId via session token é roadmap (o token
  já é emitido e validado no UDP)

## 5. Skins (modelos de NPC)

- O catálogo do menu é uma lista curada de nomes de assets do jogo (nomes
  públicos usados por todo mod de SP). Nenhum hash é fixo: tudo é resolvido
  com `GET_HASH_KEY` e validado com `IS_MODEL_IN_CDIMAGE`/`IS_MODEL_VALID`
  antes de streamar — nome errado vira mensagem no menu, nunca crash
- Trocar de skin reinicia roupas para a variação padrão do modelo
  (`SET_PED_DEFAULT_COMPONENT_VARIATION`); customização fina de roupas por
  modelo não é preservada entre trocas (roadmap)
- Modelos animais/veículos não estão no catálogo (câmera/controles de SP
  ficam quebrados com esqueleto não-humanoide); nada impede adicionar por
  conta própria se souber o nome do asset

## 6. Itens do spec não cobertos na v1

- Convites (sem integração com RGSC/Steam friends — conexão é por IP)
- Voz/chat de voz (chat de texto incluído)
- Sincronização de interiores complexos (estado de portas incluído no
  protocolo, UI ainda não expõe)
- Servidor dedicado Linux (código POSIX de sockets já isolation em Net.cpp)
