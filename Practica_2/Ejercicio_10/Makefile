EXEC = miner
CC = gcc -pthread
CFLAGS = -Wall -Wextra -g -Werror -Wpedantic -O2 -I$(INCLUDE_DIR)

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj

all: $(EXEC)

$(EXEC) : $(OBJ_DIR)/miner.o $(OBJ_DIR)/pow.o 
	@echo "Compilando $(EXEC)"
	@$(CC) $(CFALGS) -o $@ $^

$(OBJ_DIR)/pow.o: $(SRC_DIR)/pow.c
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/miner.o: $(SRC_DIR)/miner.c $(INCLUDE_DIR)/pow.h
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos generados..."
	@rm -f $(OBJ_DIR)/miner.o $(OBJ_DIR)/pow.o $(EXEC)
	@rm miners.txt target.txt votes.txt winner.txt

toss:
	@echo "Limpiando logs generados..."
	@rm *.log

run: $(EXEC)
	./$(EXEC) 0 500 30

valgrind: $(EXEC)
	valgrind -s --show-leak-kinds=all --track-origins=yes --leak-check=full ./$(EXEC) 0 10 10 > valgrind.data