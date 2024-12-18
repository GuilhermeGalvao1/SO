#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

#define MAX_FILA 100
#define MAX_TENTATIVAS 5

// Structures and type definitions
typedef struct
{
    pid_t pid;
    int hora_chegada;
    int prioridade;
    int tempo_atendimento;
} Cliente;

typedef struct Node
{
    Cliente cliente;
    struct Node *next;
} Node;

typedef struct
{
    Node *end;
    int size;
} ListaCircular;

// Global variables
sem_t *sem_atend = SEM_FAILED;
sem_t *sem_block = SEM_FAILED;
ListaCircular fila;
int total_clientes = 0, clientes_satisfeitos = 0;
struct timeval tempo_inicial;
int flag_parar = 0;
int contador_atendimentos = 0;

// Function prototypes
void initLista(ListaCircular *lista);
int isEmpty(ListaCircular *lista);
void enqueue(ListaCircular *lista, Cliente cliente);
Cliente dequeue(ListaCircular *lista);
void safe_sem_close(sem_t **sem, const char *name);
void handle_signal(int sig);
void *thread_menu(void *arg);
int verificar_arquivo_demanda(int tentativas_maximas);
void *thread_recepcao(void *arg);
void *thread_atendente(void *arg);

// List circular implementation
void initLista(ListaCircular *lista)
{
    lista->end = NULL;
    lista->size = 0;
}

int isEmpty(ListaCircular *lista)
{
    return lista->end == NULL;
}

void enqueue(ListaCircular *lista, Cliente cliente)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (!newNode)
    {
        perror("Falha ao alocar memória");
        exit(1);
    }
    newNode->cliente = cliente;

    if (isEmpty(lista))
    {
        newNode->next = newNode;
        lista->end = newNode;
    }
    else
    {
        newNode->next = lista->end->next;
        lista->end->next = newNode;
        lista->end = newNode;
    }
    lista->size++;
}

Cliente dequeue(ListaCircular *lista)
{
    if (isEmpty(lista))
    {
        perror("Tentativa de remover de uma lista vazia");
        exit(1);
    }

    Node *front = lista->end->next;
    Cliente cliente = front->cliente;

    if (front == lista->end)
    {
        lista->end = NULL;
    }
    else
    {
        lista->end->next = front->next;
    }

    free(front);
    lista->size--;
    return cliente;
}

// Utility functions
void safe_sem_close(sem_t **sem, const char *name)
{
    if (*sem != SEM_FAILED)
    {
        sem_close(*sem);
        sem_unlink(name);
        *sem = SEM_FAILED;
    }
}

void handle_signal(int sig)
{
    printf("Recebido sinal de interrupção. Encerrando...\n");
    flag_parar = 1;
}

void *thread_menu(void *arg)
{
    char ch;
    while ((ch = getchar()) != 's')
        ;
    flag_parar = 1;
    return NULL;
}

// Verificar arquivo de demanda
int verificar_arquivo_demanda(int tentativas_maximas)
{
    int tentativas = 0;
    while (tentativas < tentativas_maximas)
    {
        FILE *demanda = fopen("demanda.txt", "r");
        if (demanda != NULL)
        {
            fclose(demanda);
            return 1;
        }

        usleep(50000);
        tentativas++;
    }
    return 0;
}

// Thread de recepção
void *thread_recepcao(void *arg)
{
    int N = *(int *)arg;
    int clientes_gerados = 0;
    srand(time(NULL));

    while (!flag_parar)
    {
        if (fila.size >= MAX_FILA)
        {
            usleep(100000);
            continue;
        }

        if (N != 0 && clientes_gerados >= N)
            break;

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("Fork failed");
            break;
        }

        if (pid == 0)
        {
            execl("./cliente", "cliente", NULL);
            perror("Exec failed");
            exit(1);
        }

        sem_wait(sem_block);
        if (!verificar_arquivo_demanda(MAX_TENTATIVAS))
        {
            perror("Falha em acessar arquivo de demanda");
            kill(pid, SIGKILL);
            sem_post(sem_block);
            continue;
        }

        FILE *demanda = fopen("demanda.txt", "r");
        if (demanda == NULL)
        {
            perror("Falha ao abrir arquivo de demanda");
            kill(pid, SIGKILL);
            sem_post(sem_block);
            continue;
        }

        int tempo_atendimento = 0;
        if (fscanf(demanda, "%d", &tempo_atendimento) != 1)
        {
            perror("Falha ao ler tempo de atendimento");
            fclose(demanda);
            kill(pid, SIGKILL);
            sem_post(sem_block);
            continue;
        }
        fclose(demanda);

        struct timeval current;
        gettimeofday(&current, NULL);
        int hora_chegada = (current.tv_sec - tempo_inicial.tv_sec) * 1000 +
                           (current.tv_usec - tempo_inicial.tv_usec) / 1000;

        Cliente novo_cliente = {
            .pid = pid,
            .hora_chegada = hora_chegada,
            .prioridade = (rand() % 2 == 0) ? 1 : 0,
            .tempo_atendimento = tempo_atendimento};

        enqueue(&fila, novo_cliente);
        total_clientes++;
        printf("Cliente gerado. PID: %d\n", pid);
        printf("[DEBUG] Fila - tamanho: %d\n", fila.size);

        sem_post(sem_block);
        clientes_gerados++;

        usleep(50000);
    }

    return NULL;
}

// Thread de atendente
void *thread_atendente(void *arg)
{
    int X = *(int *)arg;
    struct timeval current;

    while (!flag_parar || fila.size > 0)
    {
        sem_wait(sem_block);

        if (isEmpty(&fila))
        {
            sem_post(sem_block);
            usleep(100000);
            continue;
        }

        Cliente cliente = dequeue(&fila);
        sem_post(sem_block);

        if (kill(cliente.pid, 0) != 0)
        {
            printf("Cliente PID %d já não existe.\n", cliente.pid);
            continue;
        }

        kill(cliente.pid, SIGCONT);

        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            struct timeval tv;
            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec;
            ts.tv_nsec = tv.tv_usec * 1000;
        }

        ts.tv_sec += 3;

        int sem_ret = sem_timedwait(sem_atend, &ts);
        if (sem_ret == -1)
        {
            if (errno == ETIMEDOUT)
            {
                printf("[TIMEOUT] Semáforo não liberado para PID: %d\n", cliente.pid);
                continue;
            }
            perror("Erro no semáforo de atendimento");
            continue;
        }

        gettimeofday(&current, NULL);
        int tempo_espera = (current.tv_sec - tempo_inicial.tv_sec) * 1000 +
                           (current.tv_usec - tempo_inicial.tv_usec) / 1000 - cliente.hora_chegada;

        int paciencia = (cliente.prioridade == 1) ? (X / 2) : X;

        if (tempo_espera <= paciencia)
            clientes_satisfeitos++;

        printf("Atendendo cliente. PID: %d, Prioridade: %d\n", cliente.pid, cliente.prioridade);

        FILE *lng = fopen("lista_numeros_gerados.txt", "a");
        if (lng)
        {
            fprintf(lng, "%d\n", cliente.pid);
            fclose(lng);
        }

        sem_post(sem_atend);

        if (++contador_atendimentos % 10 == 0)
            system("./analista &");
    }

    return NULL;
}

// Main function
int main(int argc, char *argv[])
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    sem_unlink("/sem_atend");
    sem_unlink("/sem_block");
    remove("demanda.txt");
    remove("lista_numeros_gerados.txt");

    if (argc != 3)
    {
        printf("Uso: %s <numero_clientes> <tempo_paciencia>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    int X = atoi(argv[2]);

    sem_atend = sem_open("/sem_atend", O_CREAT | O_EXCL, 0644, 1);
    sem_block = sem_open("/sem_block", O_CREAT | O_EXCL, 0644, 1);
    if (sem_atend == SEM_FAILED || sem_block == SEM_FAILED)
    {
        perror("Falha ao criar semáforos");
        return 1;
    }

    initLista(&fila);
    gettimeofday(&tempo_inicial, NULL);

    pthread_t thread_rec, thread_atend, thread_menu_t;
    pthread_create(&thread_rec, NULL, thread_recepcao, &N);
    pthread_create(&thread_atend, NULL, thread_atendente, &X);
    pthread_create(&thread_menu_t, NULL, thread_menu, NULL);

    pthread_join(thread_rec, NULL);
    pthread_join(thread_atend, NULL);
    pthread_join(thread_menu_t, NULL);

    safe_sem_close(&sem_atend, "/sem_atend");
    safe_sem_close(&sem_block, "/sem_block");

    printf("Programa finalizado. Clientes satisfeitos: %d/%d\n", clientes_satisfeitos, total_clientes);
    return 0;
}