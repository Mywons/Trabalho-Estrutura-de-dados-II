/*
 * PROJETO ECOCOLETA - QUARTA VERSAO
 * Integrantes: Ryan A, Murillo J, Davi G, Kaio F, Gabriel M,
 * Disciplina: Estrutura de Dados
 *
 * Estruturas utilizadas:
 * - Lista simplesmente encadeada: mantém todos os contentores cadastrados.
 * - Tabela hash: permite localizar contentores pelo codigo.
 * - Funcao hash: codigo % TAMANHO_TABELA.
 * - Colisoes: encadeamento separado por meio do campo proximoHash.
 * - Grafo: pontos de coleta e conexoes armazenados em matriz de adjacencia.
 * - Fila de prioridade (heap binario de maximo em vetor dinamico):
 *   organiza as solicitacoes de coleta. A prioridade maior sai primeiro
 *   (URGENTE > ALTA > NORMAL). Em caso de empate de prioridade, sai
 *   primeiro quem entrou primeiro na fila (ordem de chegada / FIFO).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define TAMANHO_TABELA 7
#define TAM_HASH TAMANHO_TABELA
#define MAX_PONTOS 20
#define TAM_NOME 60
#define INF (INT_MAX / 4)

/* Ocupacao minima (em %) para que um contentor possa entrar na fila de coleta. */
#define OCUPACAO_MINIMA_FILA 70
#define CAPACIDADE_INICIAL_FILA 4

/* Niveis de prioridade da fila de coleta. Quanto maior o valor, mais urgente. */
#define PRIORIDADE_NORMAL  1
#define PRIORIDADE_ALTA    2
#define PRIORIDADE_URGENTE 3

/* Cada contentor participa de duas estruturas:
 * proximo liga os nos da lista principal;
 * proximoHash liga os nos que colidiram na mesma posicao da tabela hash.
 */
typedef struct {
    int codigo;
    char local[50];
    char bairro[40];
    int capacidadeLitros;
    int ocupacao;
} Contentor;

typedef struct No {
    Contentor dado;
    struct No *proximo;      /* Lista principal. */
    struct No *proximoHash;  /* Cadeia da tabela hash. */
} No;

typedef struct {
    int codigo;
    char nome[TAM_NOME];
    int ativo;
} Ponto;

typedef struct {
    No *posicoes[TAMANHO_TABELA];
} TabelaHash;

/* Item da fila de prioridade: guarda apenas o codigo do contentor
 * (os dados completos sao obtidos pela tabela hash), a prioridade
 * e a ordem de chegada, usada para desempatar prioridades iguais. */
typedef struct {
    int codigo;
    int prioridade;
    unsigned long ordemChegada;
} ItemFila;

typedef struct {
    ItemFila *itens;            /* Vetor dinamico que representa o heap. */
    int tamanho;                /* Quantidade de itens na fila. */
    int capacidade;             /* Quantidade de posicoes alocadas. */
    unsigned long proximaOrdem; /* Contador de chegada (FIFO no empate). */
} FilaPrioridade;

/* Lista principal e tabela hash. */
No *listaContentores = NULL;
TabelaHash tabelaHash;

/* Fila de prioridade de coleta. */
FilaPrioridade filaColeta = { NULL, 0, 0, 0 };

/* Grafo dos pontos de coleta. Peso 0 significa que nao ha conexao. */
Ponto pontos[MAX_PONTOS];
int matriz[MAX_PONTOS][MAX_PONTOS];
int quantidadePontos = 0;

/* Leitura segura de inteiros, evitando que texto invalido trave o menu. */
int lerInteiro(const char *mensagem, int *valor) {
    char linha[128];
    char *fim;
    long numero;

    printf("%s", mensagem);
    if (fgets(linha, sizeof(linha), stdin) == NULL) return 0;
    errno = 0;
    numero = strtol(linha, &fim, 10);
    while (*fim == ' ' || *fim == '\t' || *fim == '\r' || *fim == '\n') fim++;
    if (fim == linha || *fim != '\0' || errno == ERANGE ||
        numero < INT_MIN || numero > INT_MAX) {
        printf("Entrada invalida. Digite um numero inteiro.\n");
        return 0;
    }
    *valor = (int)numero;
    return 1;
}

void lerTexto(const char *mensagem, char *destino, size_t tamanho) {
    size_t n;
    printf("%s", mensagem);
    if (fgets(destino, (int)tamanho, stdin) == NULL) {
        destino[0] = '\0';
        return;
    }
    n = strlen(destino);
    if (n > 0 && destino[n - 1] == '\n') destino[n - 1] = '\0';
    else {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
}

/* Funcao hash: transforma o codigo em um indice entre 0 e TAMANHO_TABELA - 1. */
int funcaoHash(int codigo) {
    int indice = codigo % TAMANHO_TABELA;
    if (indice < 0) indice += TAM_HASH;
    return indice;
}

/* Busca rapida na tabela hash. Colisoes sao percorridas pela cadeia proximoHash. */
No *buscarNaHash(int codigo) {
    int indice = funcaoHash(codigo);
    No *atual = tabelaHash.posicoes[indice];
    while (atual != NULL) {
        if (atual->dado.codigo == codigo) return atual;
        atual = atual->proximoHash;
    }
    return NULL;
}

/* Insere um no na tabela hash, no inicio da cadeia da posicao calculada. */
void inserirNaHash(No *novo) {
    int indice = funcaoHash(novo->dado.codigo);
    novo->proximoHash = tabelaHash.posicoes[indice];
    tabelaHash.posicoes[indice] = novo;
}

/* Retira um codigo da tabela hash sem liberar o no da lista principal. */
void removerDaHash(int codigo) {
    int indice = funcaoHash(codigo);
    No *atual = tabelaHash.posicoes[indice];
    No *anterior = NULL;
    while (atual != NULL) {
        if (atual->dado.codigo == codigo) {
            if (anterior == NULL) tabelaHash.posicoes[indice] = atual->proximoHash;
            else anterior->proximoHash = atual->proximoHash;
            atual->proximoHash = NULL;
            return;
        }
        anterior = atual;
        atual = atual->proximoHash;
    }
}

/* ===================== FILA DE PRIORIDADE (HEAP) =====================
 * O heap e uma arvore binaria guardada dentro de um vetor:
 *   - a raiz (item de maior preferencia) fica sempre em itens[0];
 *   - o pai da posicao i esta em (i - 1) / 2;
 *   - os filhos da posicao i estao em 2*i + 1 (esquerdo) e 2*i + 2 (direito).
 * Regra do heap: todo pai tem preferencia sobre seus filhos. Assim, a
 * proxima coleta e sempre itens[0], consultada em O(1). Inserir e remover
 * custam O(log n), pois o item so sobe ou desce um ramo da arvore.
 */

const char *nomePrioridade(int prioridade) {
    switch (prioridade) {
        case PRIORIDADE_NORMAL:  return "NORMAL";
        case PRIORIDADE_ALTA:    return "ALTA";
        case PRIORIDADE_URGENTE: return "URGENTE";
        default:                 return "DESCONHECIDA";
    }
}

int filaVazia(const FilaPrioridade *f) {
    return f->tamanho == 0;
}

/* Retorna 1 se o item a deve sair da fila antes do item b.
 * Criterio 1: maior prioridade. Criterio 2 (empate): menor ordem de chegada. */
int temPreferencia(const ItemFila *a, const ItemFila *b) {
    if (a->prioridade != b->prioridade) return a->prioridade > b->prioridade;
    return a->ordemChegada < b->ordemChegada;
}

void trocarItens(ItemFila *a, ItemFila *b) {
    ItemFila temp = *a;
    *a = *b;
    *b = temp;
}

/* Sobe o item da posicao i enquanto ele tiver preferencia sobre o pai. */
void subirNoHeap(FilaPrioridade *f, int i) {
    while (i > 0) {
        int pai = (i - 1) / 2;
        if (!temPreferencia(&f->itens[i], &f->itens[pai])) break;
        trocarItens(&f->itens[i], &f->itens[pai]);
        i = pai;
    }
}

/* Desce o item da posicao i ate que nenhum filho tenha preferencia sobre ele. */
void descerNoHeap(FilaPrioridade *f, int i) {
    for (;;) {
        int esquerdo = 2 * i + 1;
        int direito = 2 * i + 2;
        int maior = i;
        if (esquerdo < f->tamanho && temPreferencia(&f->itens[esquerdo], &f->itens[maior]))
            maior = esquerdo;
        if (direito < f->tamanho && temPreferencia(&f->itens[direito], &f->itens[maior]))
            maior = direito;
        if (maior == i) break;
        trocarItens(&f->itens[i], &f->itens[maior]);
        i = maior;
    }
}

/* Procura um codigo na fila. Retorna a posicao no vetor ou -1. */
int buscarNaFila(const FilaPrioridade *f, int codigo) {
    int i;
    for (i = 0; i < f->tamanho; i++)
        if (f->itens[i].codigo == codigo) return i;
    return -1;
}

/* Insere na fila. Retorna 1 em caso de sucesso e 0 se faltar memoria.
 * Se o vetor estiver cheio, a capacidade dobra com realloc (vetor dinamico).
 * O novo item entra no final e sobe ate a posicao correta. */
int inserirNaFila(FilaPrioridade *f, int codigo, int prioridade) {
    if (f->tamanho == f->capacidade) {
        int novaCapacidade = (f->capacidade == 0) ? CAPACIDADE_INICIAL_FILA : f->capacidade * 2;
        ItemFila *novo = (ItemFila *)realloc(f->itens, (size_t)novaCapacidade * sizeof(ItemFila));
        if (novo == NULL) return 0;
        f->itens = novo;
        f->capacidade = novaCapacidade;
    }
    f->itens[f->tamanho].codigo = codigo;
    f->itens[f->tamanho].prioridade = prioridade;
    f->itens[f->tamanho].ordemChegada = f->proximaOrdem++;
    f->tamanho++;
    subirNoHeap(f, f->tamanho - 1);
    return 1;
}

/* Remove o item da posicao i, mantendo a propriedade do heap.
 * O ultimo item do vetor ocupa o lugar do removido. Como ele pode ter mais
 * ou menos preferencia que a nova vizinhanca, tentamos subir e descer:
 * so um dos dois realmente movimenta o item. Para realizar a coleta,
 * i = 0 (raiz), e entao so a descida tem efeito. */
void removerDaFilaPorIndice(FilaPrioridade *f, int i) {
    if (i < 0 || i >= f->tamanho) return;
    f->tamanho--;
    if (i == f->tamanho) return; /* Era o ultimo: nada a reorganizar. */
    f->itens[i] = f->itens[f->tamanho];
    subirNoHeap(f, i);
    descerNoHeap(f, i);
}

/* Remove um codigo da fila, se estiver nela. Retorna 1 se removeu. */
int removerDaFilaPorCodigo(FilaPrioridade *f, int codigo) {
    int i = buscarNaFila(f, codigo);
    if (i == -1) return 0;
    removerDaFilaPorIndice(f, i);
    return 1;
}

void liberarFila(FilaPrioridade *f) {
    free(f->itens);
    f->itens = NULL;
    f->tamanho = 0;
    f->capacidade = 0;
    f->proximaOrdem = 0;
}

/* ======================================================================= */

void mostrarMenu(void) {
    printf("\n==================================================\n");
    printf("       SISTEMA DE CONTROLE DE CONTENTORES\n");
    printf("                     ECOCOLETA\n");
    printf("==================================================\n");
    printf("GERENCIAMENTO DE CONTENTORES\n");
    printf("1 - Cadastrar contentor\n");
    printf("2 - Buscar contentor\n");
    printf("3 - Atualizar ocupacao\n");
    printf("4 - Remover contentor\n");
    printf("5 - Listar contentores\n");
    printf("6 - Mostrar contentor mais urgente\n");
    printf("7 - Mostrar ordem de coleta\n");
    printf("\nMAPA E ROTAS\n");
    printf("8 - Cadastrar ponto\n");
    printf("9 - Cadastrar conexao entre pontos\n");
    printf("10 - Mostrar pontos e conexoes\n");
    printf("11 - Mostrar matriz de adjacencia\n");
    printf("12 - Mostrar vizinhos de um ponto\n");
    printf("13 - Calcular menor caminho entre pontos\n");
    printf("14 - Mostrar tabela hash (demonstracao de colisoes)\n");
    printf("\nFILA DE PRIORIDADE DE COLETA\n");
    printf("15 - Solicitar coleta (inserir contentor na fila)\n");
    printf("16 - Consultar proxima coleta\n");
    printf("17 - Realizar coleta\n");
    printf("18 - Mostrar fila de coleta\n");
    printf("0 - Sair\n");
    printf("==================================================\n");
}

void cadastrarContentor(void) {
    int codigo, capacidade, ocupacao;
    char local[50], bairro[40];
    No *novo;

    printf("\n--- CADASTRO DE CONTENTOR ---\n");
    if (!lerInteiro("Codigo (maior que zero): ", &codigo)) return;
    if (codigo <= 0) {
        printf("ERRO: o codigo deve ser maior que zero.\n");
        return;
    }
    if (buscarNaHash(codigo) != NULL) {
        printf("ERRO: ja existe um contentor com esse codigo.\n");
        return;
    }
    lerTexto("Local: ", local, sizeof(local));
    lerTexto("Bairro: ", bairro, sizeof(bairro));
    if (!lerInteiro("Capacidade em litros (maior que zero): ", &capacidade)) return;
    if (capacidade <= 0) {
        printf("ERRO: a capacidade deve ser maior que zero.\n");
        return;
    }
    if (!lerInteiro("Ocupacao (0 a 100%): ", &ocupacao)) return;
    if (ocupacao < 0 || ocupacao > 100) {
        printf("ERRO: a ocupacao deve estar entre 0 e 100.\n");
        return;
    }

    novo = (No *)malloc(sizeof(No));
    if (novo == NULL) {
        printf("ERRO: nao foi possivel alocar memoria.\n");
        return;
    }
    novo->dado.codigo = codigo;
    strncpy(novo->dado.local, local, sizeof(novo->dado.local) - 1);
    novo->dado.local[sizeof(novo->dado.local) - 1] = '\0';
    strncpy(novo->dado.bairro, bairro, sizeof(novo->dado.bairro) - 1);
    novo->dado.bairro[sizeof(novo->dado.bairro) - 1] = '\0';
    novo->dado.capacidadeLitros = capacidade;
    novo->dado.ocupacao = ocupacao;
    novo->proximo = listaContentores;
    novo->proximoHash = NULL;
    listaContentores = novo;
    inserirNaHash(novo);
    printf("Contentor cadastrado com sucesso. Posicao hash: %d\n", funcaoHash(codigo));
}

void imprimirContentor(const No *c) {
    if (c == NULL) return;
    printf("Codigo: %d | Local: %s | Bairro: %s | Capacidade: %d litros | Ocupacao: %d%%\n",
           c->dado.codigo, c->dado.local, c->dado.bairro,
           c->dado.capacidadeLitros, c->dado.ocupacao);
}

void buscarContentor(void) {
    int codigo;
    No *encontrado;
    if (!lerInteiro("Digite o codigo do contentor: ", &codigo)) return;
    encontrado = buscarNaHash(codigo);
    if (encontrado == NULL) printf("Contentor nao encontrado.\n");
    else {
        printf("Contentor encontrado pela tabela hash:\n");
        imprimirContentor(encontrado);
        if (buscarNaFila(&filaColeta, codigo) != -1)
            printf("Situacao: aguardando coleta na fila de prioridade.\n");
    }
}

void atualizarOcupacao(void) {
    int codigo, novaOcupacao;
    No *atual;
    if (!lerInteiro("Codigo do contentor: ", &codigo)) return;
    atual = buscarNaHash(codigo);
    if (atual == NULL) {
        printf("Contentor nao encontrado.\n");
        return;
    }
    printf("Ocupacao atual: %d%%\n", atual->dado.ocupacao);
    if (!lerInteiro("Nova ocupacao (0 a 100): ", &novaOcupacao)) return;
    if (novaOcupacao < 0 || novaOcupacao > 100) {
        printf("ERRO: a ocupacao deve estar entre 0 e 100.\n");
        return;
    }
    atual->dado.ocupacao = novaOcupacao;
    printf("Ocupacao atualizada com sucesso.\n");

    /* Se o contentor estava aguardando coleta e ficou abaixo do minimo
     * (por exemplo, esvaziado manualmente para 0%), sai da fila. */
    if (novaOcupacao < OCUPACAO_MINIMA_FILA &&
        removerDaFilaPorCodigo(&filaColeta, codigo)) {
        printf("Ocupacao abaixo de %d%%: contentor %d retirado da fila de coleta.\n",
               OCUPACAO_MINIMA_FILA, codigo);
    }
}

void removerContentor(void) {
    int codigo;
    No *atual = listaContentores;
    No *anterior = NULL;
    if (!lerInteiro("Codigo do contentor a remover: ", &codigo)) return;

    while (atual != NULL && atual->dado.codigo != codigo) {
        anterior = atual;
        atual = atual->proximo;
    }
    if (atual == NULL) {
        printf("Contentor nao encontrado.\n");
        return;
    }
    if (anterior == NULL) listaContentores = atual->proximo;
    else anterior->proximo = atual->proximo;

    removerDaHash(codigo);
    /* A fila nao pode ficar apontando para um contentor que nao existe mais. */
    if (removerDaFilaPorCodigo(&filaColeta, codigo))
        printf("Contentor %d tambem foi retirado da fila de coleta.\n", codigo);
    free(atual);
    printf("Contentor removido da lista e da tabela hash.\n");
}

void listarContentores(void) {
    No *atual = listaContentores;
    if (atual == NULL) {
        printf("Nenhum contentor cadastrado.\n");
        return;
    }
    printf("\n--- CONTENTORES CADASTRADOS ---\n");
    while (atual != NULL) {
        imprimirContentor(atual);
        atual = atual->proximo;
    }
}

void mostrarMaisUrgente(void) {
    No *atual = listaContentores;
    No *maisUrgente = NULL;
    while (atual != NULL) {
        if (maisUrgente == NULL || atual->dado.ocupacao > maisUrgente->dado.ocupacao)
            maisUrgente = atual;
        atual = atual->proximo;
    }
    if (maisUrgente == NULL) printf("Nao ha contentores cadastrados.\n");
    else {
        printf("Contentor mais urgente (maior percentual de ocupacao):\n");
        imprimirContentor(maisUrgente);
        if (maisUrgente->dado.ocupacao >= 80)
            printf("Prioridade alta: recomenda-se programar a coleta.\n");
        else printf("Ocupacao abaixo de 80%%.\n");
    }
}

/* Exibe contentores em ordem decrescente de ocupacao sem alterar a lista. */
void mostrarOrdemColeta(void) {
    No *atual;
    No **vetor = NULL;
    int n = 0, i, j;
    for (atual = listaContentores; atual != NULL; atual = atual->proximo) n++;
    if (n == 0) {
        printf("Nao ha contentores cadastrados.\n");
        return;
    }
    vetor = (No **)malloc((size_t)n * sizeof(No *));
    if (vetor == NULL) {
        printf("ERRO: memoria insuficiente para organizar a coleta.\n");
        return;
    }
    i = 0;
    for (atual = listaContentores; atual != NULL; atual = atual->proximo)
        vetor[i++] = atual;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - 1 - i; j++) {
            if (vetor[j]->dado.ocupacao < vetor[j + 1]->dado.ocupacao) {
                No *temp = vetor[j];
                vetor[j] = vetor[j + 1];
                vetor[j + 1] = temp;
            }
        }
    }
    printf("\n--- ORDEM SUGERIDA DE COLETA (MAIOR OCUPACAO PRIMEIRO) ---\n");
    for (i = 0; i < n; i++) {
        printf("%d. ", i + 1);
        imprimirContentor(vetor[i]);
    }
    free(vetor);
}

/* ================== OPERACOES DE MENU DA FILA DE COLETA ================== */

void solicitarColeta(void) {
    int codigo, prioridade;
    No *contentor;

    printf("\n--- SOLICITAR COLETA ---\n");
    if (!lerInteiro("Codigo do contentor: ", &codigo)) return;
    contentor = buscarNaHash(codigo);
    if (contentor == NULL) {
        printf("ERRO: contentor %d nao encontrado (codigo inexistente).\n", codigo);
        return;
    }
    if (contentor->dado.ocupacao < OCUPACAO_MINIMA_FILA) {
        printf("ERRO: o contentor %d esta com %d%% de ocupacao. "
               "So entram na fila contentores com ocupacao a partir de %d%%.\n",
               codigo, contentor->dado.ocupacao, OCUPACAO_MINIMA_FILA);
        return;
    }
    if (buscarNaFila(&filaColeta, codigo) != -1) {
        printf("ERRO: o contentor %d ja esta na fila de coleta (codigo repetido).\n", codigo);
        return;
    }
    printf("Prioridades: 1 - NORMAL | 2 - ALTA | 3 - URGENTE\n");
    if (!lerInteiro("Prioridade: ", &prioridade)) return;
    if (prioridade < PRIORIDADE_NORMAL || prioridade > PRIORIDADE_URGENTE) {
        printf("ERRO: prioridade invalida. Use 1, 2 ou 3.\n");
        return;
    }
    if (!inserirNaFila(&filaColeta, codigo, prioridade)) {
        printf("ERRO: nao foi possivel alocar memoria para a fila.\n");
        return;
    }
    printf("Contentor %d inserido na fila com prioridade %s. Itens na fila: %d\n",
           codigo, nomePrioridade(prioridade), filaColeta.tamanho);
}

void consultarProximaColeta(void) {
    ItemFila *topo;
    if (filaVazia(&filaColeta)) {
        printf("A fila de coleta esta vazia. Nao ha coletas pendentes.\n");
        return;
    }
    topo = &filaColeta.itens[0];
    printf("Proxima coleta (prioridade %s):\n", nomePrioridade(topo->prioridade));
    imprimirContentor(buscarNaHash(topo->codigo));
}

void realizarColeta(void) {
    ItemFila coletado;
    No *contentor;
    if (filaVazia(&filaColeta)) {
        printf("A fila de coleta esta vazia. Nao ha coleta a realizar.\n");
        return;
    }
    coletado = filaColeta.itens[0];
    removerDaFilaPorIndice(&filaColeta, 0);
    contentor = buscarNaHash(coletado.codigo);
    printf("Coleta realizada no contentor %d (prioridade %s).\n",
           coletado.codigo, nomePrioridade(coletado.prioridade));
    if (contentor != NULL) {
        printf("Ocupacao anterior: %d%%\n", contentor->dado.ocupacao);
        contentor->dado.ocupacao = 0; /* Contentor esvaziado. */
        printf("Ocupacao atualizada para 0%%.\n");
        imprimirContentor(contentor);
    }
    printf("Itens restantes na fila: %d\n", filaColeta.tamanho);
}

int compararItensFila(const void *a, const void *b) {
    const ItemFila *x = (const ItemFila *)a;
    const ItemFila *y = (const ItemFila *)b;
    if (temPreferencia(x, y)) return -1;
    if (temPreferencia(y, x)) return 1;
    return 0;
}

/* Mostra a fila na ordem em que as coletas serao feitas, sem alterar o heap. */
void mostrarFilaColeta(void) {
    ItemFila *copia;
    int i;
    if (filaVazia(&filaColeta)) {
        printf("A fila de coleta esta vazia.\n");
        return;
    }
    copia = (ItemFila *)malloc((size_t)filaColeta.tamanho * sizeof(ItemFila));
    if (copia == NULL) {
        printf("ERRO: memoria insuficiente para exibir a fila.\n");
        return;
    }
    memcpy(copia, filaColeta.itens, (size_t)filaColeta.tamanho * sizeof(ItemFila));
    qsort(copia, (size_t)filaColeta.tamanho, sizeof(ItemFila), compararItensFila);
    printf("\n--- FILA DE COLETA (%d item(ns)) ---\n", filaColeta.tamanho);
    for (i = 0; i < filaColeta.tamanho; i++) {
        No *c = buscarNaHash(copia[i].codigo);
        printf("%d. [%s] chegada #%lu - ", i + 1,
               nomePrioridade(copia[i].prioridade), copia[i].ordemChegada + 1);
        imprimirContentor(c);
    }
    free(copia);
}

/* ======================================================================= */

int indicePontoPorCodigo(int codigo) {
    int i;
    for (i = 0; i < quantidadePontos; i++)
        if (pontos[i].ativo && pontos[i].codigo == codigo) return i;
    return -1;
}

void cadastrarPonto(void) {
    int codigo;
    char nome[TAM_NOME];
    if (quantidadePontos >= MAX_PONTOS) {
        printf("Limite de %d pontos atingido.\n", MAX_PONTOS);
        return;
    }
    if (!lerInteiro("Codigo numerico do ponto: ", &codigo)) return;
    if (indicePontoPorCodigo(codigo) != -1) {
        printf("Ja existe um ponto com esse codigo.\n");
        return;
    }
    lerTexto("Nome/local do ponto: ", nome, sizeof(nome));
    if (nome[0] == '\0') {
        printf("O nome do ponto nao pode ficar vazio.\n");
        return;
    }
    pontos[quantidadePontos].codigo = codigo;
    strncpy(pontos[quantidadePontos].nome, nome, TAM_NOME - 1);
    pontos[quantidadePontos].nome[TAM_NOME - 1] = '\0';
    pontos[quantidadePontos].ativo = 1;
    quantidadePontos++;
    printf("Ponto cadastrado com sucesso.\n");
}

void cadastrarConexao(void) {
    int codigoA, codigoB, distancia, a, b;
    if (!lerInteiro("Codigo do ponto de origem: ", &codigoA)) return;
    if (!lerInteiro("Codigo do ponto de destino: ", &codigoB)) return;
    a = indicePontoPorCodigo(codigoA);
    b = indicePontoPorCodigo(codigoB);
    if (a == -1 || b == -1) {
        printf("Cadastre os dois pontos antes de criar a conexao.\n");
        return;
    }
    if (a == b) {
        printf("A conexao deve ligar dois pontos diferentes.\n");
        return;
    }
    if (!lerInteiro("Distancia/peso da conexao (maior que zero): ", &distancia)) return;
    if (distancia <= 0) {
        printf("A distancia deve ser maior que zero.\n");
        return;
    }
    matriz[a][b] = distancia;
    matriz[b][a] = distancia; /* Grafo nao direcionado. */
    printf("Conexao cadastrada nos dois sentidos.\n");
}

void mostrarPontosConexoes(void) {
    int i, j, encontrou;
    if (quantidadePontos == 0) {
        printf("Nenhum ponto cadastrado.\n");
        return;
    }
    printf("\n--- PONTOS CADASTRADOS ---\n");
    for (i = 0; i < quantidadePontos; i++)
        printf("Codigo %d - %s\n", pontos[i].codigo, pontos[i].nome);
    printf("\n--- CONEXOES ---\n");
    encontrou = 0;
    for (i = 0; i < quantidadePontos; i++) {
        for (j = i + 1; j < quantidadePontos; j++) {
            if (matriz[i][j] > 0) {
                printf("%s <-> %s | distancia/peso: %d\n",
                       pontos[i].nome, pontos[j].nome, matriz[i][j]);
                encontrou = 1;
            }
        }
    }
    if (!encontrou) printf("Nenhuma conexao cadastrada.\n");
}

void mostrarMatrizAdjacencia(void) {
    int i, j;
    if (quantidadePontos == 0) {
        printf("Cadastre pontos antes de mostrar a matriz.\n");
        return;
    }
    printf("Matriz de adjacencia (0 = sem conexao):\n     ");
    for (i = 0; i < quantidadePontos; i++) printf("%6d", pontos[i].codigo);
    printf("\n");
    for (i = 0; i < quantidadePontos; i++) {
        printf("%4d ", pontos[i].codigo);
        for (j = 0; j < quantidadePontos; j++) printf("%6d", matriz[i][j]);
        printf("\n");
    }
}

void mostrarVizinhos(void) {
    int codigo, i, encontrou = 0;
    if (!lerInteiro("Codigo do ponto: ", &codigo)) return;
    i = indicePontoPorCodigo(codigo);
    if (i == -1) {
        printf("Ponto nao encontrado.\n");
        return;
    }
    printf("Vizinhos de %s:\n", pontos[i].nome);
    for (int j = 0; j < quantidadePontos; j++) {
        if (matriz[i][j] > 0) {
            printf("- Codigo %d: %s (distancia/peso %d)\n",
                   pontos[j].codigo, pontos[j].nome, matriz[i][j]);
            encontrou = 1;
        }
    }
    if (!encontrou) printf("Esse ponto ainda nao possui conexoes.\n");
}

/* Dijkstra calcula o menor caminho em um grafo com pesos positivos. */
void menorCaminho(void) {
    int origemCodigo, destinoCodigo, origem, destino;
    int dist[MAX_PONTOS], anterior[MAX_PONTOS], visitado[MAX_PONTOS] = {0};
    int i, j;
    if (!lerInteiro("Codigo do ponto de origem: ", &origemCodigo)) return;
    if (!lerInteiro("Codigo do ponto de destino: ", &destinoCodigo)) return;
    origem = indicePontoPorCodigo(origemCodigo);
    destino = indicePontoPorCodigo(destinoCodigo);
    if (origem == -1 || destino == -1) {
        printf("Origem ou destino nao encontrado.\n");
        return;
    }
    for (i = 0; i < quantidadePontos; i++) {
        dist[i] = INF;
        anterior[i] = -1;
    }
    dist[origem] = 0;
    for (i = 0; i < quantidadePontos; i++) {
        int u = -1;
        for (j = 0; j < quantidadePontos; j++)
            if (!visitado[j] && (u == -1 || dist[j] < dist[u])) u = j;
        if (u == -1 || dist[u] == INF) break;
        visitado[u] = 1;
        for (j = 0; j < quantidadePontos; j++) {
            if (matriz[u][j] > 0 && !visitado[j] &&
                dist[u] <= INF - matriz[u][j] &&
                dist[u] + matriz[u][j] < dist[j]) {
                dist[j] = dist[u] + matriz[u][j];
                anterior[j] = u;
            }
        }
    }
    if (dist[destino] == INF) {
        printf("Nao existe caminho entre os pontos informados.\n");
        return;
    }
    {
        int caminho[MAX_PONTOS], tamanho = 0, atual = destino;
        while (atual != -1 && tamanho < MAX_PONTOS) {
            caminho[tamanho++] = atual;
            atual = anterior[atual];
        }
        printf("Menor caminho: ");
        for (i = tamanho - 1; i >= 0; i--) {
            printf("%s", pontos[caminho[i]].nome);
            if (i > 0) printf(" -> ");
        }
        printf("\nDistancia/peso total: %d\n", dist[destino]);
    }
}

/* Exibe como os codigos estao distribuidos na tabela hash, inclusive colisoes. */
void mostrarTabelaHash(void) {
    int i;
    printf("\n--- TABELA HASH (TAM=%d) ---\n", TAMANHO_TABELA);
    for (i = 0; i < TAM_HASH; i++) {
        No *atual = tabelaHash.posicoes[i];
        printf("Indice %d: ", i);
        if (atual == NULL) printf("vazio");
        while (atual != NULL) {
            printf("[%d]", atual->dado.codigo);
            if (atual->proximoHash != NULL) printf(" -> ");
            atual = atual->proximoHash;
        }
        printf("\n");
    }
    printf("Codigos com o mesmo indice ficam encadeados: isso trata as colisoes.\n");
}

void liberarMemoria(void) {
    No *atual = listaContentores;
    int liberados = 0;
    while (atual != NULL) {
        No *proximo = atual->proximo;
        free(atual);
        liberados++;
        atual = proximo;
    }
    listaContentores = NULL;
    for (int i = 0; i < TAMANHO_TABELA; i++) tabelaHash.posicoes[i] = NULL;
    liberarFila(&filaColeta);
    printf("%d contentor(es) liberado(s) e fila de coleta desalocada.\n", liberados);
}

int main(void) {
    int opcao;
    {
        int i;
        for (i = 0; i < TAMANHO_TABELA; i++) tabelaHash.posicoes[i] = NULL;
    }
    do {
        mostrarMenu();
        if (!lerInteiro("Digite a opcao: ", &opcao)) {
            if (feof(stdin)) { /* Fim da entrada: encerra sem entrar em laco infinito. */
                opcao = 0;
                printf("\nFim da entrada.\n");
                liberarMemoria();
                printf("Memoria liberada. Sistema encerrado.\n");
                break;
            }
            opcao = -1;
            continue;
        }
        switch (opcao) {
            case 1: cadastrarContentor(); break;
            case 2: buscarContentor(); break;
            case 3: atualizarOcupacao(); break;
            case 4: removerContentor(); break;
            case 5: listarContentores(); break;
            case 6: mostrarMaisUrgente(); break;
            case 7: mostrarOrdemColeta(); break;
            case 8: cadastrarPonto(); break;
            case 9: cadastrarConexao(); break;
            case 10: mostrarPontosConexoes(); break;
            case 11: mostrarMatrizAdjacencia(); break;
            case 12: mostrarVizinhos(); break;
            case 13: menorCaminho(); break;
            case 14: mostrarTabelaHash(); break;
            case 15: solicitarColeta(); break;
            case 16: consultarProximaColeta(); break;
            case 17: realizarColeta(); break;
            case 18: mostrarFilaColeta(); break;
            case 0:
                liberarMemoria();
                printf("Memoria liberada. Sistema encerrado.\n");
                break;
            default: printf("Opcao inexistente. Tente novamente.\n");
        }
    } while (opcao != 0);
    return 0;
}
