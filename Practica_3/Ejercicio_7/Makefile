EXEC = miner
CC = gcc -pthread
CFLAGS = -Wall -Wextra -g -lrt -Werror -Wpedantic -O2 -I$(INCLUDE_DIR)
# el -lrt sirve para la memoria compartida
UNAME := $(shell uname -s)

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj

all: $(EXEC)

$(EXEC) : $(OBJ_DIR)/miner.o $(OBJ_DIR)/pow.o
	@echo "Compilando $(EXEC)"
	@$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/pow.o: $(SRC_DIR)/pow.c
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/miner.o: $(SRC_DIR)/miner.c $(INCLUDE_DIR)/pow.h
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos generados..."
	@pkill -x $(EXEC) 2>/dev/null; true
	@rm -f $(OBJ_DIR)/miner.o $(OBJ_DIR)/pow.o $(EXEC)
	@rm -f miners.txt target.txt votes.txt winner.txt participants.txt starter.txt round_size.txt tiempos.data *.log
ifeq ($(UNAME), Darwin)
	@python3 -c "import ctypes; ctypes.CDLL(None).sem_unlink(b'/miners_mutex')" 2>/dev/null; true
else
	@rm -f /dev/shm/sem.miners_mutex
endif

run: $(EXEC)
	./$(EXEC) 10 8000 & ./$(EXEC) 20 8000 & ./$(EXEC) 30 8000


toss:
	@echo "Limpiando logs generados..."
	@rm *.log

valgrind: $(EXEC)
	valgrind -s --show-leak-kinds=all --track-origins=yes --leak-check=full ./$(EXEC) 0 10 10 > valgrind.data