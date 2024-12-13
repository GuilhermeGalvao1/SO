#ifndef UTIL_H
// verifica se não está definida

#define UTIL_H
// define a macro UTIL_H

#include <stdbool.h>
#include <sys/types.h>

// Macro útil
#define MAX_BUFFER_SIZE 1024

// Estrutura do cliente
typedef struct
{
    pid_t pid;             // Identificador do processo
    int hora_chegada;      // Hora de chegada em milissegundos
    int prioridade;        // 0: baixa, 1: alta
    int tempo_atendimento; // Tempo necessário para atendimento
} Cliente;

// Funções utilitárias
bool is_empty(char *fila);
bool is_prime(int number);
void exibir_cliente(const Cliente *cliente);

#endif // fim da verificação