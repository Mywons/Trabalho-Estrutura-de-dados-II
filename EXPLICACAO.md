# EcoColeta: explicação do código

Guia de estudo do grupo. O professor pode chamar **qualquer integrante** para explicar o
funcionamento, então todos devem ler até o fim. Os nomes entre `crases` são nomes reais de
funções e variáveis do `ecocoleta.c`. Procure-os no arquivo enquanto lê.

---

## 1. O que o programa faz

O programa gerencia contentores de lixo de uma cidade:

1. **Cadastra** contentores (código, local, bairro, capacidade, % de ocupação).
2. **Busca, atualiza, remove e lista** contentores.
3. Mantém um **mapa** de pontos de coleta e calcula o **menor caminho** entre eles.
4. Mantém uma **fila de prioridade de coleta**, que decide qual contentor é esvaziado primeiro.

Tudo é controlado por um **menu** em `main`, num laço `do { ... } while (opcao != 0)`.
Cada opção chama uma função própria (`switch (opcao)`).

---

## 2. As estruturas de dados (visão geral)

| Estrutura | Para que serve | Variável global |
|---|---|---|
| Lista simplesmente encadeada | Guarda **todos** os contentores | `listaContentores` |
| Tabela hash (7 posições) | Encontra um contentor pelo código sem percorrer a lista | `tabelaHash` |
| Grafo em matriz de adjacência | Pontos de coleta e distâncias entre eles | `pontos`, `matriz` |
| Fila de prioridade (heap) | Ordem em que as coletas serão feitas | `filaColeta` |

### Ponto-chave: um contentor = um nó só

```c
typedef struct No {
    Contentor dado;           // os dados do contentor
    struct No *proximo;       // próximo nó da LISTA
    struct No *proximoHash;   // próximo nó na mesma posição da HASH
} No;
```

Cada contentor é alocado **uma única vez** com `malloc`, e esse mesmo nó participa de duas
estruturas ao mesmo tempo:
- pelo ponteiro `proximo`, ele faz parte da lista;
- pelo ponteiro `proximoHash`, ele faz parte da tabela hash.

Assim os dados não são duplicados. A fila de prioridade também não copia o contentor: ela guarda só
o **código** e usa a hash para buscar o resto.

---

## 3. Leitura segura do teclado

### `lerInteiro(mensagem, &valor)`
- Lê a linha inteira com `fgets` e converte com `strtol`.
- Se o usuário digitar texto (exemplo: `abc`), número quebrado (`12x`) ou número grande demais,
  mostra "Entrada invalida" e retorna 0.
- **Por que não usar `scanf("%d")`?** Se você digita uma letra, o `scanf` não consome a entrada e
  o menu entra em laço infinito. Com `fgets` a linha sempre é consumida.
- Retorna 0 também quando a entrada acaba (EOF). A `main` usa `feof(stdin)` para detectar isso e
  encerrar o programa liberando a memória.

### `lerTexto(mensagem, destino, tamanho)`
- Lê com `fgets` e remove o `\n` do final.
- Se o texto for maior que o espaço disponível, descarta o resto da linha. Assim a sobra não é lida
  como resposta da próxima pergunta.

---

## 4. Lista encadeada de contentores

| Função | O que faz |
|---|---|
| `cadastrarContentor` | Valida os dados, aloca um `No` com `malloc`, insere **no início** da lista e na hash |
| `listarContentores` | Percorre a lista do início ao fim imprimindo cada nó |
| `removerContentor` | Percorre a lista guardando o `anterior`, religa os ponteiros, tira da hash e da fila e dá `free` |

**Validações do cadastro:** o código deve ser maior que 0 e não pode estar repetido (checado
na hash). A capacidade deve ser maior que 0 e a ocupação deve ficar entre 0 e 100.

**Inserção no início** é O(1): o novo nó aponta para o antigo primeiro, e `listaContentores`
passa a apontar para o novo.

**Remoção:**
```
anterior -> atual -> proximo      vira      anterior -> proximo
```
Se o removido for o primeiro (`anterior == NULL`), a própria `listaContentores` passa a apontar
para o segundo.

### Outras funções que usam a lista
- `mostrarMaisUrgente`: percorre a lista e guarda o contentor de maior ocupação. Avisa se
  passou de 80%.
- `mostrarOrdemColeta`: copia os ponteiros para um vetor auxiliar, ordena com **bubble sort**
  (maior ocupação primeiro), imprime e dá `free` no vetor. A lista original não é alterada.
  *Atenção:* isso é só uma **sugestão** por ocupação. A ordem real de coleta é a da fila de
  prioridade (seção 7).

---

## 5. Tabela hash

### Função hash
```c
indice = codigo % TAMANHO_TABELA;   // TAMANHO_TABELA = 7
```
Exemplos: 101 % 7 = **3**, 102 % 7 = **4**, 105 % 7 = **0**.

### Colisões (encadeamento separado)
Dois códigos podem cair no mesmo índice. Exemplo: 3 e 10, porque 10 % 7 = 3. Nesse caso, os
nós ficam numa **cadeia** ligada pelo campo `proximoHash`:

```
Indice 3: [10] -> [3]
```

| Função | O que faz | Custo médio |
|---|---|---|
| `inserirNaHash` | Coloca o nó no **início** da cadeia do índice | O(1) |
| `buscarNaHash` | Vai ao índice e percorre só aquela cadeia | O(1) |
| `removerDaHash` | Desliga o nó da cadeia. **Não dá `free`**: quem libera é `removerContentor` | O(1) |
| `mostrarTabelaHash` (opção 14) | Mostra cada índice e suas cadeias, deixando as colisões visíveis | |

**Por que usar hash se já existe a lista?** Buscar na lista é O(n): pode ser preciso olhar todos os nós.
Com a hash, olhamos só a cadeia de um índice.

`buscarContentor` (opção 2) e `atualizarOcupacao` (opção 3) usam a hash para achar o contentor.

---

## 6. Grafo: mapa e rotas

- `pontos[]`: vetor com até `MAX_PONTOS` (20) pontos de coleta (código + nome).
- `matriz[i][j]`: **matriz de adjacência**. Vale 0 quando não há ligação; quando há, guarda a distância
  entre o ponto `i` e o ponto `j`.
- O grafo é **não direcionado**: `cadastrarConexao` grava `matriz[a][b]` **e** `matriz[b][a]`.

| Função | O que faz |
|---|---|
| `indicePontoPorCodigo` | Converte o código do ponto no índice do vetor/matriz (ou -1) |
| `cadastrarPonto` (8) | Adiciona ponto; impede código repetido e nome vazio |
| `cadastrarConexao` (9) | Liga dois pontos existentes e diferentes com distância > 0 |
| `mostrarPontosConexoes` (10) | Lista pontos e cada conexão uma vez (`j = i + 1`) |
| `mostrarMatrizAdjacencia` (11) | Imprime a matriz inteira |
| `mostrarVizinhos` (12) | Percorre a linha do ponto na matriz: valor > 0 indica vizinho |
| `menorCaminho` (13) | **Algoritmo de Dijkstra** |

### Dijkstra, passo a passo (`menorCaminho`)
1. `dist[]` começa com `INF` para todos e `0` na origem. `anterior[]` começa com -1.
2. Repete: escolhe o ponto **não visitado** com menor `dist` (`u`) e marca como visitado.
3. Para cada vizinho `j` de `u`: se `dist[u] + peso < dist[j]`, atualiza `dist[j]` e guarda
   `anterior[j] = u`. Isso se chama **relaxamento**.
4. No fim, `dist[destino]` é a menor distância. O caminho é montado **de trás para frente**
   seguindo `anterior[]` a partir do destino e depois impresso na ordem certa.
5. Se `dist[destino]` continuar `INF`, não existe caminho.

`INF` é `INT_MAX / 4`, e há o teste `dist[u] <= INF - matriz[u][j]`. Os dois servem para evitar
**overflow** na soma.

Dijkstra só funciona com pesos **positivos**. Por isso a distância precisa ser maior que 0.

---

## 7. Fila de prioridade de coleta (parte nova)

### Regras do enunciado
| Regra | Onde é verificada |
|---|---|
| Código precisa existir | `solicitarColeta` → `buscarNaHash` |
| Ocupação **≥ 70%** para entrar | `solicitarColeta` → `OCUPACAO_MINIMA_FILA` |
| Não pode repetir código na fila | `solicitarColeta` → `buscarNaFila` |
| Prioridades 1 = NORMAL, 2 = ALTA, 3 = URGENTE | `solicitarColeta` / `nomePrioridade` |
| Empate: sai quem chegou primeiro | `temPreferencia` usando `ordemChegada` |

### Estruturas
```c
typedef struct {
    int codigo;                  // qual contentor
    int prioridade;              // 1, 2 ou 3
    unsigned long ordemChegada;  // 0, 1, 2, ... (para desempatar)
} ItemFila;

typedef struct {
    ItemFila *itens;             // vetor dinâmico (o heap)
    int tamanho;                 // quantos itens há
    int capacidade;              // quantos cabem sem realocar
    unsigned long proximaOrdem;  // contador de chegada
} FilaPrioridade;
```

### Quem sai primeiro? (`temPreferencia`)
1. Quem tem **maior prioridade**.
2. Se a prioridade for igual, quem tem **menor `ordemChegada`**, ou seja, quem chegou antes (FIFO).

### O heap
É uma **árvore binária guardada num vetor**, sem ponteiros:

```
índice:   0     1     2     3     4
        [103] [102] [101] [104] ...

              103 (raiz = próxima coleta)
             /    \
          102      101
          /
        104
```

- pai de `i` → `(i - 1) / 2`
- filho esquerdo de `i` → `2*i + 1`
- filho direito de `i` → `2*i + 2`
- **Regra:** todo pai tem preferência sobre seus filhos. Então a **raiz (`itens[0]`) é sempre a
  próxima coleta**.

### Operações
| Opção | Função | Como funciona | Custo |
|---|---|---|---|
| 15 | `solicitarColeta` → `inserirNaFila` | Se o vetor estiver cheio, dobra com `realloc`. Coloca o item **no fim** e chama `subirNoHeap`: enquanto o item tiver preferência sobre o pai, troca os dois | O(log n) |
| 16 | `consultarProximaColeta` | Só lê `itens[0]`. **Não remove** | O(1) |
| 17 | `realizarColeta` | Guarda a raiz, coloca o **último** item na posição 0 e chama `descerNoHeap`: troca o item com o filho de maior preferência até ele ficar no lugar certo. Depois **zera a ocupação** do contentor | O(log n) |
| 18 | `mostrarFilaColeta` | Copia o vetor, ordena a **cópia** com `qsort` e imprime. O heap original fica intacto | O(n log n) |

### Remover um item do meio (`removerDaFilaPorIndice`)
O último item ocupa o buraco. Ele pode ser "melhor" ou "pior" que os novos vizinhos, por isso
chamamos `subirNoHeap` **e** `descerNoHeap`; só um dos dois realmente move o item.

### Integração com o resto do sistema
- **Excluir contentor (opção 4):** `removerContentor` também chama `removerDaFilaPorCodigo`. Sem
  isso, a fila guardaria o código de um nó que já levou `free`.
- **Atualizar ocupação para menos de 70% (opção 3)**, por exemplo para 0: o contentor sai da fila,
  porque não precisa mais de coleta.
- **Buscar contentor (opção 2):** avisa se o contentor está aguardando coleta.

### Por que heap e não uma lista ordenada?
| | Lista ordenada | Heap |
|---|---|---|
| Inserir | O(n) (procurar a posição) | O(log n) |
| Retirar o primeiro | O(1) | O(log n) |
| Consultar o primeiro | O(1) | O(1) |

Com muitas inserções, o heap é mais eficiente. A lista ordenada também seria aceitável.

---

## 8. Memória: quem aloca e quem libera

| O que | Aloca | Libera |
|---|---|---|
| Nó de contentor | `malloc` em `cadastrarContentor` | `free` em `removerContentor` ou `liberarMemoria` |
| Vetor do heap | `realloc` em `inserirNaFila` | `free` em `liberarFila` |
| Vetor auxiliar da ordem de coleta | `malloc` em `mostrarOrdemColeta` | `free` no fim da mesma função |
| Cópia da fila para exibir | `malloc` em `mostrarFilaColeta` | `free` no fim da mesma função |

**Encerramento (opção 0):** `liberarMemoria` percorre a lista dando `free` em cada nó. Antes de
liberar, guarda o `proximo`, porque depois do `free` não se pode mais acessar o nó. Em seguida,
zera as posições da hash e chama `liberarFila`.

O programa foi testado com **valgrind**, que acusou: *"All heap blocks were freed -- no leaks are
possible"*.

---

## 9. Os 11 testes exigidos

Todos estão em `testes/entrada_testes.txt`. Para rodar no OnlineGDB, cole esse arquivo na aba
**stdin**. O detalhamento de cada teste está em `TESTES.md`.

| Teste | Resultado esperado |
|---|---|
| Fila vazia | Opções 16, 17 e 18 avisam que a fila está vazia |
| Código inexistente | 999 é recusado |
| Ocupação abaixo de 70% | 105 (40%) é recusado |
| Prioridades normal, alta, urgente | 101/N, 102/A e 103/U entram; 103 vai para o topo |
| Mesma prioridade | 102/A e 104/A: 102 sai antes porque chegou antes |
| Código repetido na fila | 101 de novo é recusado |
| Consulta da próxima coleta | Mostra 103 sem remover |
| Realização da coleta | Coleta 103, depois 102 |
| Atualização da ocupação para 0 | Após a coleta, 103 fica com 0%; 106 zerado na opção 3 sai da fila |
| Exclusão de contentor na fila | Remover 104 também o tira da fila |
| Encerramento e liberação | "5 contentor(es) liberado(s) e fila de coleta desalocada" |

---

## 10. Perguntas que o professor pode fazer

**Por que a fila guarda só o código, e não o contentor inteiro?**
Para não duplicar dados. Se a ocupação mudar, existe uma única cópia do contentor para atualizar.
A hash encontra o contentor pelo código rapidamente.

**O que acontece se dois contentores forem URGENTES?**
Sai primeiro o que foi inserido antes. Cada inserção recebe um número de chegada crescente
(`ordemChegada`).

**O que é uma colisão na hash e como vocês trataram?**
É quando dois códigos geram o mesmo índice. Tratamos com encadeamento separado: os nós desse
índice formam uma lista ligada pelo campo `proximoHash`.

**Por que a remoção da hash não dá `free`?**
O nó também está na lista principal. Quem libera é `removerContentor`, uma única vez, depois de
tirar o nó da lista, da hash e da fila. Liberar duas vezes daria erro (*double free*).

**Por que o tamanho da hash é 7?**
É um número primo, o que ajuda a espalhar melhor os códigos. Ele é pequeno de propósito, para
mostrar colisões na apresentação.

**O que é o `realloc` na fila?**
É o que torna o vetor dinâmico. Quando o vetor enche, a capacidade dobra (4 → 8 → 16...), então a
fila não tem limite fixo.

**Por que `(i - 1) / 2` é o pai?**
Pela forma como a árvore é guardada no vetor: os filhos de `i` estão em `2i+1` e `2i+2`. Fazendo a
conta ao contrário (com divisão inteira), chega-se ao pai.

**Como o Dijkstra sabe o caminho, e não só a distância?**
Pelo vetor `anterior[]`, que guarda de onde viemos para chegar a cada ponto. Basta seguir esse
vetor a partir do destino até a origem.

**Por que usar `fgets` em vez de `scanf`?**
Se o usuário digitar uma letra, o `scanf("%d")` deixa a letra presa na entrada e o menu entra em
laço infinito. Com `fgets` + `strtol`, a linha inteira é lida e validada.
