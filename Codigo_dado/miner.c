#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pow.h"

int main(int argc, char* argv[]) {
  int target, n_rounds, n_threads, i, res = 0;
  pid_t pid;

  if (argc != 4) {
    return -1;
  }

  target = atoi(argv[1]);
  n_rounds = atoi(argv[2]);
  n_threads = atoi(argv[3]);

  /*Creamos el fork*/
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) {
    logger("poner algo");
    exit(EXIT_SUCCESS);

  } else if (pid > 0) {
    res = miner(target, n_rounds, n_threads);
  }

  wait(NULL);
  exit(EXIT_SUCCESS);

  return 0;
}

int miner(int target, int n_rounds, int n_threads) {
  int i = 0;
  int* hilos; /*RESERVAR MEMORIA*/

  for (i = 0; i < n_threads; i++) {
    hilos[i] = pthread_create();
  }

  for (i = 0; i < n_threads; i++) {
    pthread_join(hilos[i]);
  }

  return EXIT_SUCCESS;
}

int logger(char* filename) { return EXIT_SUCCESS; }
