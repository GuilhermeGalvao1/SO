#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define MAX_FILA 100

// Estrutura do cliente
typedef struct
{
    pid_t pid;
    int hora_chegada;
    int prioridade;
    int tempo_atendimento;
} Cliente;

// Variáveis globais
sem_t *sem_atend;
sem_t *sem_block;
Cliente fila[MAX_FILA];
int fila_in = 0, fila_out = 0;
int total_clientes = 0, clientes_satisfeitos = 0;
int tempo_inicial, flag_parar = 0;
int contador_atendimentos = 0;

// Threads
void *thread_recepcao(void *arg);
void *thread_atendente(void *arg);
void *thread_menu(void *arg);

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Uso: %s <numero_clientes> <tempo_paciencia>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    int X = atoi(argv[2]);

    // Inicialização dos semáforos
    sem_atend = sem_open("/sem_atend", O_CREAT | O_EXCL, 0644, 1);
    if (sem_atend == SEM_FAILED)
    {
        sem_unlink("/sem_atend");
        sem_atend = sem_open("/sem_atend", O_CREAT | O_EXCL, 0644, 1);
    }

    sem_block = sem_open("/sem_block", O_CREAT | O_EXCL, 0644, 1);
    if (sem_block == SEM_FAILED)
    {
        sem_unlink("/sem_block");
        sem_block = sem_open("/sem_block", O_CREAT | O_EXCL, 0644, 1);
    }

    // Registro do tempo inicial
    tempo_inicial = time(NULL);

    // Criação das threads
    pthread_t thread_rec, thread_atend, thread_menu_t;
    pthread_create(&thread_rec, NULL, thread_recepcao, &N);
    pthread_create(&thread_atend, NULL, thread_atendente, &X);
    pthread_create(&thread_menu_t, NULL, thread_menu, NULL);

    // Espera das threads
    pthread_join(thread_rec, NULL);
    pthread_join(thread_atend, NULL);
    pthread_join(thread_menu_t, NULL);

    // Limpeza de semáforos
    sem_close(sem_atend);
    sem_unlink("/sem_atend");
    sem_close(sem_block);
    sem_unlink("/sem_block");

    return 0;
}

void *thread_recepcao(void *arg)
{
    int N = *(int *)arg;
    int clientes_gerados = 0;

    while (1)
    {
        // Condição de parada para número finito de clientes
        if (N != 0 && clientes_gerados >= N)
            break;

        // Verificar limite da fila
        if (fila_in - fila_out >= MAX_FILA)
        {
            usleep(100000); // Espera se fila estiver cheia
            continue;
        }

        // Gerar processo cliente
        pid_t pid = fork();
        if (pid == 0)
        { // Processo filho (cliente)
            execl("./cliente", "cliente", NULL);
            exit(1);
        }

        // Configurar cliente na fila
        sem_wait(sem_block);
        FILE *demanda = fopen("demanda.txt", "r");
        int tempo_atendimento;
        fscanf(demanda, "%d", &tempo_atendimento);
        fclose(demanda);

        fila[fila_in % MAX_FILA] = (Cliente){
            .pid = pid,
            .hora_chegada = time(NULL) - tempo_inicial,
            .prioridade = (rand() % 2 == 0) ? 1 : 0,
            .tempo_atendimento = tempo_atendimento};
        fila_in++;
        total_clientes++;
        sem_post(sem_block);

        clientes_gerados++;

        printf("Cliente gerado. PID: %d\n", pid);

        if (N == 0 && flag_parar)
            break;
    }

    return NULL;
}

void *thread_atendente(void *arg)
{
    int X = *(int *)arg;

    while (1)
    {
        // Verificar se há clientes na fila
        if (fila_in == fila_out)
        {
            if (flag_parar)
                break;
            usleep(100000);
            continue;
        }

        sem_wait(sem_block);
        Cliente cliente = fila[fila_out % MAX_FILA];
        sem_post(sem_block);

        // Acordar cliente
        if (kill(cliente.pid, 0) == 0) // Verifica se o processo existe
        {
            kill(cliente.pid, SIGCONT);
        }
        else
        {
            printf("Cliente PID %d já não existe.\n", cliente.pid);
        }

        sem_wait(sem_atend);

        // Calcular satisfação
        int tempo_espera = time(NULL) - tempo_inicial - cliente.hora_chegada;
        int paciencia = (cliente.prioridade == 1) ? (X / 2) : X;

        if (tempo_espera <= paciencia)
        {
            clientes_satisfeitos++;
        }

        // Escrever PID no arquivo Lista de Números Gerados
        sem_wait(sem_block);
        FILE *lng = fopen("lista_numeros_gerados.txt", "a");
        fprintf(lng, "%d\n", cliente.pid);
        fclose(lng);
        sem_post(sem_block);

        fila_out++;

        // Incrementar contador de atendimentos
        contador_atendimentos++;

        printf("Atendendo cliente. PID: %d, Prioridade: %d\n",
               cliente.pid, cliente.prioridade);

        // Acordar Analista a cada 10 atendimentos
        if (contador_atendimentos % 10 == 0)
        {
            system("./analista &");
        }
    }

    // Caso reste algum cliente não impresso, acionar analista
    if (contador_atendimentos % 10 != 0)
    {
        system("./analista &");
    }

    // Imprimir resultados
    float taxa_satisfacao = (float)clientes_satisfeitos / total_clientes;
    int tempo_total = time(NULL) - tempo_inicial;
    printf("Taxa de Satisfação: %.2f\n", taxa_satisfacao);
    printf("Tempo Total: %d segundos\n", tempo_total);

    return NULL;
}

void *thread_menu(void *arg)
{
    char ch;
    while ((ch = getchar()) != 's')
        ;
    flag_parar = 1;
    return NULL;
}