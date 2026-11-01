# EcoColeta — Roteiro de testes da fila de prioridade

Como executar (no OnlineGDB, cole o conteúdo de `testes/entrada_testes.txt` na aba **stdin**;
ou no terminal):

```
gcc -std=c99 -Wall -Wextra -o ecocoleta ecocoleta.c
./ecocoleta < testes/entrada_testes.txt
```

A saída completa está em `testes/saida_testes.txt`. Também foi executado com
`valgrind --leak-check=full`: **"All heap blocks were freed -- no leaks are possible"**.

Regras da fila: só entram contentores com ocupação **≥ 70%**; prioridade 1 = NORMAL,
2 = ALTA, 3 = URGENTE; sai primeiro a maior prioridade e, em empate, quem chegou primeiro.
Implementação: heap binário de máximo em vetor dinâmico (`realloc`).

Contentores cadastrados: 101 (90%), 102 (85%), 103 (95%), 104 (75%), 105 (40%), 106 (80%).

| # | Teste exigido | Ação (menu) | Resultado obtido |
|---|---|---|---|
| 1 | Fila vazia | 16, 17 e 18 antes de qualquer inserção | "A fila de coleta esta vazia..." nas três opções |
| 2 | Código inexistente | 15 → código 999 | "contentor 999 nao encontrado (codigo inexistente)" |
| 3 | Ocupação abaixo de 70% | 15 → 105 (40%) | Recusado: "So entram na fila contentores com ocupacao a partir de 70%" |
| 4 | Prioridades normal, alta e urgente | 15 → 101/NORMAL, 102/ALTA, 103/URGENTE | Os três inseridos; 103 fica no topo |
| 5 | Dois contentores com a mesma prioridade | 15 → 104/ALTA (depois do 102/ALTA) | Fila: 103, 102, 104, 101 — 102 sai antes de 104 (chegou antes) |
| 6 | Código repetido na fila | 15 → 101 de novo | "ja esta na fila de coleta (codigo repetido)" |
| 7 | Consulta da próxima coleta | 16 | Mostra 103 (URGENTE) sem remover |
| 8 | Realização da coleta | 17 (duas vezes) | Coleta 103, depois 102; a próxima passa a ser 104 |
| 9 | Atualização da ocupação para 0 | Após a coleta: 2 → 103 mostra 0%. Também: 106 entra como URGENTE e 3 → 106 → 0 | Ocupação zerada; 106 é retirado da fila por ficar abaixo de 70% |
| 10 | Exclusão de contentor presente na fila | 4 → 104 | "Contentor 104 tambem foi retirado da fila de coleta"; fila fica só com 101 |
| 11 | Encerramento e liberação da memória | 0 | "5 contentor(es) liberado(s) e fila de coleta desalocada"; valgrind sem vazamentos |
