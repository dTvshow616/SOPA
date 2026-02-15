#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pow.h"

typedef struct {
  int target; /*Objetivo a alcanzar*/
  int start;
  int end;              /*Inicio y fin del rango para este hilo*/
  atomic_int* found;    /*Flag para indicar si se encontró solución*/
  atomic_int* solution; /*Solución encontrada por este hilo*/
} thread_args_t;

/**
 * Funciones privadas
 */
int miner_round(int target, int n_threads);
static void* mine_worker(void* arg);
int parentMiner(int target, int n_rounds, int n_threads);
int childLogger(char* filename);

int main(int argc, char* argv[]) {
  int target, n_rounds, n_threads, res = 0;
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
    // childLogger("poner algo");
    printf("Child process (logger) started with PID %d\n", getpid());
    exit(EXIT_SUCCESS);

  } else if (pid > 0) {
    res = parentMiner(target, n_rounds, n_threads);
    if (res != EXIT_SUCCESS) {
      fprintf(stderr, "Error: parentMiner failed\n");
      exit(EXIT_FAILURE);
    }
  }

  wait(NULL);
  exit(EXIT_SUCCESS);

  return 0;
}

/**
 * Minero completo: hace n_rounds rondas, y cada ronda el siguiente target
 * pasa a ser la solución encontrada.
 */
int parentMiner(int target, int n_rounds, int n_threads) {
  int sol = 0;
  int r = 1;

  for (r = 1; r <= n_rounds; r++) {
    sol = miner_round(target, n_threads);
    if (sol < 0) {
      fprintf(stderr, "Error: no se encontró solución en ronda %d\n", r);
      return EXIT_FAILURE;
    }
    /*Para comprobar el apartado b)*/
    printf("Solution accepted : %08d --> %d\n", target, sol);

    target = sol; /*siguiente objetivo para la siguiente ronda = solución actual*/
  }
  return EXIT_SUCCESS;
}

/*Función que ejecutan los hilos, se ponen a probar los valores posibles dentro del rango que se les asignó
y si encuentran la solución, marcan found=1 y guardan la solución en solution*/
static void* mine_worker(void* arg) {
  thread_args_t* a = (thread_args_t*)arg;
  int x, expected = 0;

  for (x = a->start; x < a->end; x++) {
    /*Si otro hilo ya encontró la solución, sale*/
    if (atomic_load(a->found) == 1) {
      return NULL;
    }

    /*Calcula hash y compara con target*/
    if (pow_hash(x) == a->target) {
      /*Marcar que ha encontrado solucion marcando found=1

      Si g_found vale 0, entonces cámbialo a 1 y devuelve true.
      Si g_found NO vale 0, NO lo cambies y devuelve false. Expected es el valor que se espera que tenga g_found
      para que se realice el cambio. En este caso, se espera que g_found sea 0 para cambiarlo a 1.
      Si g_found ya es 1, entonces no se realiza el cambio y la función devuelve false.
      */

      expected = 0;
      if (atomic_compare_exchange_strong(a->found, &expected, 1)) {
        atomic_store(a->solution, x);
      }
      return NULL;
    }
  }

  return NULL;
}

/**
 * Resuelve UNA ronda (un target) usando n_threads hilos.
 * Devuelve la solución encontrada, o -1 si no se encontró.
 */
int miner_round(int target, int n_threads) {
  pthread_t* threads = NULL;
  thread_args_t* args = NULL;

  atomic_int found = 0;
  atomic_int solution = -1;

  int i, j, chunk, rem, start, end, extra, err, sol = 0;

  if (n_threads <= 0) {
    return -1;
  }

  /*Reservar memoria*/
  threads = malloc(sizeof(pthread_t) * (size_t)n_threads);
  args = malloc(sizeof(thread_args_t) * (size_t)n_threads);
  if (!threads || !args) {
    free(threads);
    free(args);
    return -1;
  }

  /*División del espacio [0, POW_LIMIT) entre n_threads*/
  chunk = POW_LIMIT / n_threads;
  rem = POW_LIMIT % n_threads;

  start = 0;

  /*Para cada hilo, calcular su rango de trabajo*/
  for (i = 0; i < n_threads; i++) {
    extra = (i < rem) ? 1 : 0;
    end = start + chunk + extra;

    args[i].target = target;
    args[i].start = start;
    args[i].end = end;
    args[i].found = &found;
    args[i].solution = &solution;

    /*Crear el hilo y ponerlo a trabajar en su rango*/
    err = pthread_create(&threads[i], NULL, mine_worker, &args[i]);

    /*Si falla crear un hilo: marcamos found para frenar a los ya creados*/
    if (err != 0) {
      atomic_store(&found, 1);

      /*Esperamos a los que sí se crearon*/
      for (j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }

      free(threads);
      free(args);
      return -1;
    }
    start = end;
  }

  /*Esperamos a todos (cuando found=1, los demás saldrán solos)*/
  for (i = 0; i < n_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  /*Marcar solución encontrada*/
  sol = atomic_load(&solution);

  free(threads);
  free(args);
  return sol;
}

int childLogger(char* filename) { return EXIT_SUCCESS; }
