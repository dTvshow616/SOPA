EXEC1 = miner
EXEC2 = monitor
CC = gcc
CFLAGS = -Wall -Wextra -g -Werror -Wpedantic -O2 -I$(INCLUDE_DIR)
LDFLAGS = -pthread -lrt # El -lrt sirve para la memoria compartida y las colas de mensajes
UNAME := $(shell uname -s)

# Directorios
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj

all: $(EXEC1) $(EXEC2)

$(EXEC1): $(OBJ_DIR)/miner.o $(OBJ_DIR)/pow.o
	@echo "Compilando $(EXEC1)"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EXEC2): $(OBJ_DIR)/monitor.o $(OBJ_DIR)/pow.o
	@echo "Compilando $(EXEC2)"
	@$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/pow.o: $(SRC_DIR)/pow.c
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/miner.o: $(SRC_DIR)/miner.c $(INCLUDE_DIR)/pow.h $(INCLUDE_DIR)/miner_rush.h
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/monitor.o: $(SRC_DIR)/monitor.c $(INCLUDE_DIR)/pow.h $(INCLUDE_DIR)/miner_rush.h
	@echo "Compilando: $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Limpiando archivos generados y recursos del sistema..."
	@pkill -x $(EXEC1) 2>/dev/null; true
	@pkill -x $(EXEC2) 2>/dev/null; true
	@rm -f $(OBJ_DIR)/*.o $(EXEC1) $(EXEC2)
	@rm -f *.log
	@rm -f /dev/shm/*shared_memory*
	@rm -f /dev/mqueue/*message_queue*
ifeq ($(UNAME), Darwin)
	@python3 -c "import ctypes; ctypes.CDLL(None).sem_unlink(b'/semaphore')" 2>/dev/null; true
else
	@rm -f /dev/shm/sem.semaphore*
endif

run: all
	@echo "Ejecutando monitor (en segundo plano)"
	./$(EXEC2) 10 10 &
	@sleep 1
	@echo "Soltando a los mineros"
	./$(EXEC1) 5 8000 & ./$(EXEC1) 10 8000 & ./$(EXEC1) 15 8000

toss:
	@echo "Limpiando logs generados..."
	@rm -f *.log

# No le metas muchos hilos a valgrind porque no puede soportarlo
valgrind: all
	@echo "Ejecutando monitor (en segundo plano) con valgrind"
	valgrind --leak-check=full --track-origins=yes --trace-children=yes --log-file="valgrind_monitor.log" ./monitor 500 1000 &
	@sleep 2
	@echo "Soltando a los mineros con valgrind"
	valgrind --leak-check=full --track-origins=yes --trace-children=yes --log-file="valgrind_miner1.log" ./miner 5 5 &
	valgrind --leak-check=full --track-origins=yes --trace-children=yes --log-file="valgrind_miner2.log" ./miner 10 5 &
	valgrind --leak-check=full --track-origins=yes --trace-children=yes --log-file="valgrind_miner3.log" ./miner 15 5