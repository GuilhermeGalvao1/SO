#include "util.h"
#include <stdio.h>
#include <math.h>

// Implementação de funções utilitárias

bool is_empty(char *fila)
{
    return false;
}

bool is_prime(int number)
{
    if (number < 2)
    {
        return false;
    }
    for (int i = 2; i <= sqrt(number); i++)
    {
        if (number % i == 0)
        {
            return false;
        }
    }
    return true;
}

void exibir_cliente(const Cliente *cliente)
{
    printf("PID: %d\n", cliente->pid);
    printf("Hora de chegada: %d ms\n", cliente->hora_chegada);
    printf("Prioridade: %s\n", cliente->prioridade == 0 ? "Baixa" : "Alta");
    printf("Tempo de atendimento: %d ms\n", cliente->tempo_atendimento);
}