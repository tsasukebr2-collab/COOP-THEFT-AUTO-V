# Protocolo de rede — GTAV-Coop v1

## Transporte

- TCP: controle. Porta padrão 31999.
- UDP: estado. Porta padrão 31998.

## Frames

### TCP
```
[u8 protoVer=1][u8 msgId][u16 payloadLen LE][payload]
```

### UDP
```
[u8 protoVer=1][u8 msgId][u32 clientId LE][u64 sessionToken LE][payload]
```
O token valida o remetente no servidor (anti-spoof). Endpoint UDP do cliente
é aprendido no primeiro pacote.

## Mensagens TCP

| ID | Nome | Direção | Payload |
|----|------|---------|---------|
| 1 | HELLO | C→S | protoVer u32, clientVer u32, name[8] |
| 2 | WELCOME | S→C | clientId u32, hostId u32, token u64, saveVer u64, saveBlob |
| 3 | JOIN_REJECT | S→C | reason u8, text[8] |
| 4 | PLAYER_JOINED | S→C | clientId u32, name[8] |
| 5 | PLAYER_LEFT | S→C | clientId u32, reason u8 |
| 6 | CHAT | ambos | clientId u32, text[8] |
| 7 | MISSION_STATE | host→C | missionHash u64, state u8, checkpoint u8, slot u8 |
| 8 | SAVE_SYNC | host→S→C | version u64, blob |
| 9 | SPAWN_REQUEST | C→S | entityType u8, modelHash u64, tag u32, pos, yaw |
| 10 | SPAWN | S→C | guid u64, entityType u8, modelHash u64, tag u32, pos, yaw |
| 11 | DESPAWN | ambos | guid u64, reason u8 |
| 12 | CLIENT_SUMMARY | C→S→C | missionFlag u8, wanted u8, paused u8, cutscene u8, slot u8 |
| 13 | SESSION_STATE | S→C | playerCount u32, hostId u32, saveVer u64 |
| 14 | KICK | S→C | reason u8 |
| 15 | EVENT_EXPLOSION | ambos | pos, radius f32, type u8, ownerGuid u64 |
| 16 | PLAYER_MODEL | C→S→C | clientId u32 (carimbado pelo servidor), modelHash u64 (0 = protagonista) |

Notas: `PLAYER_JOINED` (4) carrega `modelHash` no final do payload — quem
entra depois já vê as skins escolhidas pelos outros. O servidor guarda a
última skin por cliente e a carimba no PLAYER_JOINED e no relay do
PLAYER_MODEL (o `clientId` do remetente é sempre sobrescrito — anti-spoof).

## Mensagens UDP

| ID | Nome | Payload |
|----|------|---------|
| 32 | MOVE | clientId u32, tick u32, pos(i32×3 mm), yaw u16, vel(i16×3), health u16, armour u8, slot u8, inVehicle u8, vehicleGuid u64, seat u8, weaponHash u64, ammo u16, dead u8 |
| 33 | PING | nonce u64 |
| 34 | PONG | nonce u64, serverTick u32 |
| 35 | VEHICLE | guid u64, ownerId u32, pos, yaw u16, pitch f32, roll f32, vel, engineHP f32, bodyHP f32, flags u8, tyreMask u8, driverGuid u64 |
| 36 | NPC | guid u64, ownerId u32, pos, yaw u16, health u16, dead u8, npcType u8 |
| 37 | WORLD | tick u32, clockH u8, clockM u8, clockS u8, weather u8, wanted u8, paused u8 |

## Quantização

- Posição: int32, resolução 1 mm (alcance ±2147 km)
- Velocidade: int16, 1/64 m/s
- Yaw: uint16 em círculo completo (0.0055°)
- Quartile-size MOVE: ~40 bytes → 20 Hz ≈ 0.8 kB/s por jogador

## Regras de autoridade

1. Missão: só o host envia MSG_MISSION_STATE; servidor descarta de outros
2. Save: só o host envia MSG_SAVE_SYNC; servidor versiona e guarda
3. Mundo (MSG_WORLD): só do host
4. Veículo: motorista transmite; outros aplicam com lerp + snap > 15 m
5. Spawns: host media via SPAWN/SPAWN_REQUEST com GUIDs (owner<<32|local)
