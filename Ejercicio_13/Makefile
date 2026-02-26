EXEC = miner
CC = gcc
CFALGS = -Wall -g -Werror -Wpedantic -ansi -O2

all: $(EXEC)

$(EXEC) : miner.o pow.o 
	@echo "Compilando $(EXEC)"
	@$(CC) $(CFALGS) -o $@ $^

miner.o: miner.c pow.h
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

pow.o: pow.c
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos generados..."
	@rm -f miner.o pow.o $(EXEC)

run: $(EXEC)
	./$(EXEC) 0 500 10

valgrind: $(EXEC)
	valgrind -s --show-leak-kinds=all --track-origins=yes --leak-check=full ./$(EXEC)