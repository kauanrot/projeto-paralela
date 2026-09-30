/*
 * log_analyzer_par.c - Versao paralela com estado compartilhado e MUTEX GLOBAL
 * Uso: ./log_analyzer_par <arquivo_de_log> <num_threads> [tamanho_do_bloco_em_bytes]
 *
 * Divisao do trabalho: o arquivo e dividido em blocos de bytes. A thread i processa
 * os blocos i, i + N, i + 2N, ... (N = numero de threads). Se o tamanho do bloco nao
 * for informado, cada thread recebe um unico bloco do tamanho (arquivo / N).
 *
 * Sincronizacao: todas as threads atualizam a MESMA estrutura global. Para cada linha
 * lida, a thread trava o mutex, atualiza as estatisticas e destrava (secao critica).
 */
/* ---- Funcoes de leitura, contagem e relatorio (iguais nas tres versoes) ---- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_ITENS 1000   /* maximo de URLs (e de IPs) diferentes */
#define TAM_NOME 120

/* Guarda um nome (URL ou IP) e quantas vezes ele apareceu */
typedef struct {
    char nome[TAM_NOME];
    long long quantidade;
} Contador;

/* Todas as estatisticas (Nivel 1 e Nivel 2 + navegadores) */
typedef struct {
    long long total;
    long long total_200;
    long long total_404;
    long long total_erros;      /* status >= 400 */
    long long total_bytes;      /* apenas requisicoes 200 */
    long long por_hora[24];
    long long por_status[10];   /* 200 301 302 400 403 404 500 502 503 outros */
    long long por_metodo[5];    /* GET POST PUT DELETE outros */
    long long por_navegador[6]; /* Chrome Firefox Safari Edge Bots Outros */
    Contador urls[MAX_ITENS];
    int qtd_urls;
    Contador ips[MAX_ITENS];
    int qtd_ips;
} Estatisticas;

/* Dados de uma unica linha do log */
typedef struct {
    char ip[TAM_NOME];
    char url[TAM_NOME];
    int hora;
    int status;
    int metodo;
    int navegador;
    long long bytes;
} Registro;

int codigos_status[9] = {200, 301, 302, 400, 403, 404, 500, 502, 503};
char *nomes_status[10] = {"200 OK", "301 Moved", "302 Found", "400 Bad Req", "403 Forbidden",
                          "404 Not Found", "500 Internal", "502 Bad Gateway", "503 Unavail", "Outros"};
char *nomes_metodo[5] = {"GET", "POST", "PUT", "DELETE", "OUTROS"};
char *nomes_navegador[6] = {"Chrome", "Firefox", "Safari", "Edge", "Bots", "Outros"};

/* Zera todas as estatisticas */
void iniciar_estatisticas(Estatisticas *e) {
    memset(e, 0, sizeof(Estatisticas));
}

/* Descobre o navegador olhando o texto do User-Agent.
   A ordem importa: o Edge tambem contem "Chrome" e o Chrome contem "Safari". */
int descobrir_navegador(const char *ua) {
    if (strstr(ua, "bot") != NULL || strstr(ua, "Bot") != NULL) return 4;
    if (strstr(ua, "Edg") != NULL) return 3;
    if (strstr(ua, "Chrome") != NULL) return 0;
    if (strstr(ua, "Firefox") != NULL) return 1;
    if (strstr(ua, "Safari") != NULL) return 2;
    return 5;
}

/* Le uma linha do log e preenche o registro. Retorna 1 se deu certo, 0 se a linha for invalida.
   Formato: IP - - [15/Sep/2025:15:30:00 -0300] "GET /index.html HTTP/1.1" 200 1500 "User-Agent" */
int ler_linha(const char *linha, Registro *r) {
    char metodo[20];
    const char *p;
    const char *q;

    /* IP: primeiro campo */
    if (sscanf(linha, "%119s", r->ip) != 1) return 0;

    /* Hora: numero depois do primeiro ':' dentro dos colchetes */
    p = strchr(linha, '[');
    if (p == NULL) return 0;
    if (sscanf(p, "[%*[^:]:%d:", &r->hora) != 1) return 0;
    if (r->hora < 0 || r->hora > 23) return 0;

    /* Requisicao entre aspas: metodo e url */
    p = strchr(linha, '"');
    if (p == NULL) return 0;
    if (sscanf(p, "\"%19s %119s", metodo, r->url) != 2) return 0;

    /* Depois da aspa que fecha a requisicao vem o status e os bytes */
    q = strchr(p + 1, '"');
    if (q == NULL) return 0;
    r->bytes = 0;   /* se os bytes forem "-" continua 0 */
    if (sscanf(q + 1, "%d %lld", &r->status, &r->bytes) < 1) return 0;

    /* User-Agent: entre o proximo par de aspas */
    q = strchr(q + 1, '"');
    if (q == NULL) r->navegador = 5;
    else r->navegador = descobrir_navegador(q + 1);

    /* Indice do metodo */
    if (strcmp(metodo, "GET") == 0) r->metodo = 0;
    else if (strcmp(metodo, "POST") == 0) r->metodo = 1;
    else if (strcmp(metodo, "PUT") == 0) r->metodo = 2;
    else if (strcmp(metodo, "DELETE") == 0) r->metodo = 3;
    else r->metodo = 4;

    return 1;
}

/* Soma 'n' na contagem do nome (cria o nome se ainda nao existir) */
void contar_item(Contador *lista, int *qtd, const char *nome, long long n) {
    for (int i = 0; i < *qtd; i++) {
        if (strcmp(lista[i].nome, nome) == 0) {
            lista[i].quantidade += n;
            return;
        }
    }
    if (*qtd < MAX_ITENS) {
        strcpy(lista[*qtd].nome, nome);
        lista[*qtd].quantidade = n;
        (*qtd)++;
    }
}

/* Atualiza as estatisticas com uma linha */
void atualizar_estatisticas(Estatisticas *e, Registro *r) {
    int indice_status = 9;   /* "outros" */
    for (int i = 0; i < 9; i++) {
        if (r->status == codigos_status[i]) indice_status = i;
    }

    e->total++;
    if (r->status == 200) {
        e->total_200++;
        e->total_bytes += r->bytes;
    }
    if (r->status == 404) e->total_404++;
    if (r->status >= 400) e->total_erros++;

    e->por_hora[r->hora]++;
    e->por_status[indice_status]++;
    e->por_metodo[r->metodo]++;
    e->por_navegador[r->navegador]++;
    contar_item(e->urls, &e->qtd_urls, r->url, 1);
    contar_item(e->ips, &e->qtd_ips, r->ip, 1);
}

/* Soma as estatisticas de 'origem' dentro de 'destino' (usada na reducao local) */
void somar_estatisticas(Estatisticas *destino, Estatisticas *origem) {
    destino->total += origem->total;
    destino->total_200 += origem->total_200;
    destino->total_404 += origem->total_404;
    destino->total_erros += origem->total_erros;
    destino->total_bytes += origem->total_bytes;
    for (int i = 0; i < 24; i++) destino->por_hora[i] += origem->por_hora[i];
    for (int i = 0; i < 10; i++) destino->por_status[i] += origem->por_status[i];
    for (int i = 0; i < 5; i++) destino->por_metodo[i] += origem->por_metodo[i];
    for (int i = 0; i < 6; i++) destino->por_navegador[i] += origem->por_navegador[i];
    for (int i = 0; i < origem->qtd_urls; i++)
        contar_item(destino->urls, &destino->qtd_urls, origem->urls[i].nome, origem->urls[i].quantidade);
    for (int i = 0; i < origem->qtd_ips; i++)
        contar_item(destino->ips, &destino->qtd_ips, origem->ips[i].nome, origem->ips[i].quantidade);
}

/* Descobre o tamanho do arquivo em bytes */
long tamanho_do_arquivo(const char *nome_arquivo) {
    FILE *f = fopen(nome_arquivo, "r");
    if (f == NULL) {
        printf("Erro ao abrir o arquivo %s\n", nome_arquivo);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long tamanho = ftell(f);
    fclose(f);
    return tamanho;
}

/* Escreve um numero com separador de milhar: 1234567 -> 1,234,567 */
void formatar(long long n, char *saida) {
    char texto[32];
    int j = 0;
    sprintf(texto, "%lld", n);
    int tam = strlen(texto);
    for (int i = 0; i < tam; i++) {
        if (i > 0 && (tam - i) % 3 == 0) saida[j++] = ',';
        saida[j++] = texto[i];
    }
    saida[j] = '\0';
}

/* Funcao de comparacao para o qsort: maior quantidade primeiro (empate: ordem alfabetica) */
int comparar_contadores(const void *a, const void *b) {
    const Contador *x = a;
    const Contador *y = b;
    if (x->quantidade > y->quantidade) return -1;
    if (x->quantidade < y->quantidade) return 1;
    return strcmp(x->nome, y->nome);
}

/* Imprime uma linha "nome   12,345 (12.34%)" */
void imprimir_linha(const char *nome, long long valor, long long total) {
    char numero[32];
    formatar(valor, numero);
    printf("%-16s %14s (%.2f%%)\n", nome, numero, total > 0 ? 100.0 * valor / total : 0.0);
}

void imprimir_top10(Contador *lista, int qtd, const char *unidade) {
    char numero[32];
    qsort(lista, qtd, sizeof(Contador), comparar_contadores);
    for (int i = 0; i < 10 && i < qtd; i++) {
        formatar(lista[i].quantidade, numero);
        printf("%2d. %-28s %s %s\n", i + 1, lista[i].nome, numero, unidade);
    }
}

/* Imprime o relatorio no formato do enunciado */
void imprimir_relatorio(Estatisticas *e, const char *arquivo, int threads, long bloco, double tempo) {
    char numero[32];
    long long total = e->total;

    printf("============================================================\n");
    printf("ANALISADOR DE LOGS - RELATORIO COMPLETO\n");
    printf("============================================================\n\n");
    printf("ARQUIVO: %s\n", arquivo);
    printf("THREADS: %d\n", threads);
    if (bloco > 0) printf("BLOCO: %ld bytes\n", bloco);
    printf("TEMPO DE EXECUCAO: %.6f segundos\n\n", tempo);

    printf("------------------------------------------------------------\n");
    printf("ESTATISTICAS BASICAS\n");
    printf("------------------------------------------------------------\n");
    formatar(e->total, numero);
    printf("Total de Requisicoes: %s\n", numero);
    formatar(e->total_200, numero);
    printf("Requisicoes 200 (OK): %s (%.2f%%)\n", numero, total > 0 ? 100.0 * e->total_200 / total : 0.0);
    formatar(e->total_404, numero);
    printf("Requisicoes 404 (Not Found): %s (%.2f%%)\n", numero, total > 0 ? 100.0 * e->total_404 / total : 0.0);
    formatar(e->total_bytes, numero);
    printf("Total de Bytes (200): %s\n", numero);
    formatar(total > 0 ? e->total_bytes / total : 0, numero);
    printf("Media de Bytes/Req: %s bytes\n", numero);
    printf("Taxa de Erro Geral: %.2f%%\n\n", total > 0 ? 100.0 * e->total_erros / total : 0.0);

    printf("------------------------------------------------------------\n");
    printf("TOP 10 URLs MAIS ACESSADAS\n");
    printf("------------------------------------------------------------\n");
    imprimir_top10(e->urls, e->qtd_urls, "acessos");

    printf("\n------------------------------------------------------------\n");
    printf("TOP 10 IPS MAIS ATIVOS\n");
    printf("------------------------------------------------------------\n");
    imprimir_top10(e->ips, e->qtd_ips, "requisicoes");

    printf("\n------------------------------------------------------------\n");
    printf("DISTRIBUICAO POR HORA\n");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < 24; i++) {
        char nome[8];
        sprintf(nome, "%02dh", i);
        imprimir_linha(nome, e->por_hora[i], total);
    }

    printf("\n------------------------------------------------------------\n");
    printf("DISTRIBUICAO DE CODIGOS DE STATUS\n");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < 10; i++) imprimir_linha(nomes_status[i], e->por_status[i], total);

    printf("\n------------------------------------------------------------\n");
    printf("ANALISE DE METODOS HTTP\n");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < 5; i++) imprimir_linha(nomes_metodo[i], e->por_metodo[i], total);

    printf("\n------------------------------------------------------------\n");
    printf("ANALISE DE USER-AGENT\n");
    printf("------------------------------------------------------------\n");
    for (int i = 0; i < 6; i++) imprimir_linha(nomes_navegador[i], e->por_navegador[i], total);

    printf("\n============================================================\n");
    printf("FIM DO RELATORIO\n");
    printf("============================================================\n");
}
#include <pthread.h>

Estatisticas *global;                 /* estatisticas compartilhadas */
pthread_mutex_t mutex;                /* protege a estrutura global */

/* Dados que cada thread recebe */
typedef struct {
    int id;
    int num_threads;
    const char *arquivo;
    long tamanho_arquivo;
    long tamanho_bloco;
} DadosThread;

void *processar(void *arg) {
    DadosThread *dados = (DadosThread *)arg;
    char linha[2000];
    Registro registro;

    FILE *f = fopen(dados->arquivo, "r");
    if (f == NULL) return NULL;

    for (long b = dados->id; b * dados->tamanho_bloco < dados->tamanho_arquivo; b += dados->num_threads) {
        long inicio = b * dados->tamanho_bloco;
        long fim = inicio + dados->tamanho_bloco;
        long posicao = inicio;

        if (inicio > 0) {
            /* Volta 1 byte e le ate o fim dessa linha: descarta a linha parcial
               (ela pertence ao bloco anterior). Se o bloco ja comeca no inicio de
               uma linha, o byte anterior e '\n' e so ele e descartado. */
            fseek(f, inicio - 1, SEEK_SET);
            if (fgets(linha, sizeof(linha), f) == NULL) continue;
            posicao = inicio - 1 + strlen(linha);
        } else {
            fseek(f, 0, SEEK_SET);
        }

        /* Processa as linhas que comecam antes do fim do bloco */
        while (posicao < fim && fgets(linha, sizeof(linha), f) != NULL) {
            posicao += strlen(linha);
            if (ler_linha(linha, &registro)) {
                pthread_mutex_lock(&mutex);      /* entra na secao critica */
                atualizar_estatisticas(global, &registro);
                pthread_mutex_unlock(&mutex);    /* sai da secao critica */
            }
        }
    }

    fclose(f);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Uso: %s <arquivo_de_log> <num_threads> [tamanho_do_bloco]\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[2]);
    if (num_threads < 1) {
        printf("O numero de threads deve ser pelo menos 1\n");
        return 1;
    }

    long tamanho_arquivo = tamanho_do_arquivo(argv[1]);
    long tamanho_bloco = 0;
    if (argc > 3) tamanho_bloco = atol(argv[3]);
    if (tamanho_bloco <= 0) tamanho_bloco = (tamanho_arquivo + num_threads - 1) / num_threads;
    if (tamanho_bloco < 1) tamanho_bloco = 1;

    global = malloc(sizeof(Estatisticas));
    iniciar_estatisticas(global);
    pthread_mutex_init(&mutex, NULL);

    pthread_t threads[num_threads];
    DadosThread dados[num_threads];
    struct timespec inicio, fim;

    clock_gettime(CLOCK_MONOTONIC, &inicio);

    for (int i = 0; i < num_threads; i++) {
        dados[i].id = i;
        dados[i].num_threads = num_threads;
        dados[i].arquivo = argv[1];
        dados[i].tamanho_arquivo = tamanho_arquivo;
        dados[i].tamanho_bloco = tamanho_bloco;
        pthread_create(&threads[i], NULL, processar, &dados[i]);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    pthread_mutex_destroy(&mutex);
    imprimir_relatorio(global, argv[1], num_threads, tamanho_bloco, tempo);
    free(global);
    return 0;
}
