#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>

int main()
{
    sem_t *sem_block = sem_open("/sem_block", O_RDWR);
    if (sem_block == SEM_FAILED)
    {
        perror("sem_open failed");
        return 1; // semaphore opened incorrectly
        // check if this influences the rest of the processes***************
    }

    sem_wait(sem_block);
    FILE *lng = fopen("lista_numeros_gerados.txt", "r");
    if (lng == NULL)
    {
        perror("fopen failed");
        sem_post(sem_block);
        return 1; // file can't be opened
    }

    raise(SIGSTOP); // critital region - is it necessary do raise SIGSTOP?

    int count = 0;
    int pid;
    while (count < 10 && fscanf(lng, "%d", &pid) == 1)
    {
        printf("PID: %d\n", pid);
        count++;
    }

    // Truncar arquivo após imprimir
    fclose(lng);
    lng = fopen("lista_numeros_gerados.txt", "w");
    fclose(lng);

    sem_post(sem_block);

    return 0;
}