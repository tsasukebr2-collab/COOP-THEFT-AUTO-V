# Testes

## Automatizados (`coop_tests.exe`, rodam sem o jogo)

| Suite | Cobre |
|---|---|
| protocol | framing TCP (single/chunked/multi-frame/partial), rejeição de lixo, bounds-check do Reader, quantização pos/vel/yaw (ida-e-volta + wrap), roundtrip MOVE, VEHICLE e PLAYER_MODEL byte-exato, PLAYER_JOINED com modelHash, CRC32 (vetor padrão "123456789" → 0xCBF43926), GUID owner/local |
| save | roundtrip de seções, guard de versão (v9 não sobrescreve v10), recuperação por backup após corrupção de bytes, rejeição limpa de arquivo inválido |

Rode: `scripts\build.ps1 -Test`

## Manuais (duas instâncias do GTA V — sua máquina)

Checklist por fase, execute na ordem:

### Fase 2 — conexão
1. [ ] Servidor sobe e escuta (log `Server up`)
2. [ ] Host conecta, recebe WELCOME + save v1
3. [ ] Cliente conecta, ambos veem PLAYER_JOINED
4. [ ] Ping aparece no chip CO-OP (≈ RTT real)
5. [ ] Kick do cliente (fechar jogo) aparece como PLAYER_LEFT no host
6. [ ] Host sai → migração de host (log `host migrated`)

### Fase 3 — personagens/veículos
7. [ ] Ped do remoto aparece com modelo do protagonista correto
8. [ ] Movimento suave (interp 100 ms), snap só em teleporte
9. [ ] Entrar num carro: dono transmite estado, remoto vê dano/pneus
10. [ ] Motorista sai → veículo congela no remoto (sem drift)
11. [ ] F9 → Skins → aplicar modelo de NPC: remoto vê o ped trocar de modelo
12. [ ] Novo cliente conecta depois: vê a skin já aplicada nos outros
13. [ ] "Original" restaura o protagonista (e o remoto vê a restauração)

### Fase 4 — mundo
14. [ ] Host muda hora → cliente segue (snap > 18 min de drift)
15. [ ] Host muda clima → cliente vê
16. [ ] Nível de procurado replica host→cliente
17. [ ] Explosão (ex. RPG) replica

### Fase 5 — missões
18. [ ] Host inicia missão suportada → cliente vê STARTING→ACTIVE
19. [ ] Checkpoint replica
20. [ ] Fim de missão replica COMPLETED (ou FAILED)
21. [ ] Missão unsupported → clientes avisados, sem sync fake

### Fase 6 — save
22. [ ] Autosave 2 min → servidor loga `shared save updated`
23. [ ] Reiniciar servidor + reconectar → progresso mantém
24. [ ] Matar servidor no meio de escrita (kill -9) → backup recupera
25. [ ] Cliente com save local mais novo (crash) → mantém o dele e empurra

## Resultados

Os testes automatizados precisam rodar numa máquina com compilador
(esta máquina não tem MSVC/CMake). Preencha abaixo ao rodar:

```
> coop_tests.exe
GTAV-Coop test suite: XX checks, 0 failures
```
