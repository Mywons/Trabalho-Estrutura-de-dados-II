# EcoColeta — Guia para explicar o código

Este guia ajuda o grupo a explicar o programa. O professor pode chamar qualquer integrante.

## 1. Visão geral: quatro estruturas trabalhando juntas

| Estrutura | Para que serve | Onde está |
|---|---|---|
| Lista simplesmente encadeada | Guarda **todos** os contentores (campo `proximo`) | `listaContentores`, `cadastrarContentor`, `removerContentor` |
| Tabela hash (tamanho 7) | Acha um contentor pelo código sem percorrer a lista | `funcaoHash`, `buscarNaHash`, `inserirNaHash`, `removerDaHash` |
| Grafo (matriz de adjacência) | Pontos de coleta, conexões e menor caminho (Dijkstra) | `matriz`, `cadastrarConexao`, `menorCaminho` |
| **Fila de prioridade (heap)** | Decide **qual contentor coletar primeiro** | `filaColeta`, funções `...Fila` e `...Heap` |

Cada contentor é **um único nó** (`No`) alocado com `malloc`. A lista e a hash apontam para o
mesmo nó. A fila guarda só o **código** e usa a hash para buscar os dados.

## 2. Tabela hash
- `funcaoHash(codigo) = codigo % 7`. Exemplo: 101 % 7 = 3, então o contentor 101 fica no índice 3.
- **Colisão**: dois códigos no mesmo índice (exemplo: 3 e 10) ficam encadeados pelo campo
  `proximoHash`. Isso se chama encadeamento separado. A opção 14 mostra as colisões.

## 3. Fila de prioridade (parte nova)

### Regras
- Só entra contentor **cadastrado** e com ocupação **≥ 70%** (`OCUPACAO_MINIMA_FILA`).
- Um código não pode estar duas vezes na fila.
- Prioridades: 1 = NORMAL, 2 = ALTA, 3 = URGENTE. Sai primeiro a maior.
- **Empate**: sai primeiro quem chegou antes. Cada item recebe um número de chegada
  (`ordemChegada`) que só aumenta. Função `temPreferencia`.

### Por que heap?
É uma árvore binária guardada num vetor:
- raiz em `itens[0]`, que é sempre a próxima coleta;
- pai de `i` = `(i-1)/2`, filhos de `i` = `2i+1` e `2i+2`;
- todo pai tem preferência sobre os filhos.

| Operação | Como funciona | Custo |
|---|---|---|
| Consultar próxima (16) | Lê `itens[0]` | O(1) |
| Inserir (15) | Põe no fim do vetor e **sobe** trocando com o pai (`subirNoHeap`) | O(log n) |
| Coletar (17) | Tira a raiz, põe o último no lugar e **desce** trocando com o filho preferido (`descerNoHeap`). Zera a ocupação | O(log n) |
| Remover um do meio | Procura o código, põe o último no lugar, sobe/desce | O(n) busca + O(log n) |

O vetor é **dinâmico**: começa com 4 posições e dobra com `realloc` quando enche.

### Ligação com o resto do sistema
- **Excluir contentor (opção 4)**: também tira da fila. Senão a fila guardaria um código de
  um nó já liberado com `free`.
- **Atualizar ocupação para menos de 70% (opção 3)**, por exemplo para 0: o contentor sai da fila.
- **Mostrar fila (18)**: copia o vetor, ordena a cópia com `qsort` e imprime. O heap original não muda.

## 4. Liberação de memória (opção 0)
`liberarMemoria` percorre a lista dando `free` em cada nó, zera a tabela hash e chama
`liberarFila` (`free` no vetor do heap). Testado com valgrind: nenhum vazamento.

## 5. Perguntas prováveis
- **Por que a fila guarda só o código?** Para não duplicar dados. A hash acha o contentor em O(1) em média.
- **O que acontece com dois URGENTES?** Sai o que foi inserido primeiro (`ordemChegada` menor).
- **Por que não ordenar uma lista a cada inserção?** Lista ordenada insere em O(n). O heap insere e remove em O(log n).
- **Por que `lerInteiro` usa `fgets` + `strtol`?** Para que um texto digitado no lugar de um
  número não trave o menu.
