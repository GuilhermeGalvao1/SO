CC = gcc
CFLAGS = -pthread

TARGETS = atendimento cliente analista
TEMP_FILES = demanda.txt lista_numeros_gerados.txt spid.txt

all: $(TARGETS)

atendimento: atendimento.c
	$(CC) $(CFLAGS) -o $@ $<

cliente: cliente.c
	$(CC) $(CFLAGS) -o $@ $<

analista: analista.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS) $(TEMP_FILES)

run:
	./atendimento 10 1000

run-infinite:
	./atendimento 0 1000

.PHONY: all clean run run-infinite
