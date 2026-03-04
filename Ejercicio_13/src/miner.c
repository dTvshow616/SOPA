/**
 * @file miner.c
 * @author Carlos Mendez & Ana Olsson
 * @brief Miner Rush program
 * @version 5.0
 * @date 2026-02-25
 */

/* La descripcion de las librerias a continuacion se ha sacado del man siempre que se podia */
#include <fcntl.h>    /* manipulate file descriptor */
#include <pthread.h>  /* POSIX threads */
#include <stdint.h>   /* exact-width integer types */
#include <stdio.h>    /* standard input/output library functions */
#include <stdlib.h>   /* numeric conversion functions, pseudo-random numbers generation functions, memory allocation, process control functions */
#include <sys/wait.h> /* wait for process to change state */
#include <unistd.h>   /* POSIX operating system API */

#include "pow.h" /* Librería interna para el POW */

/**
 * @brief Estructura para pasar argumentos a los hilos
 */
typedef struct {
  int target; /* Objetivo a alcanzar */
  int start;  /* Inicio del rango para este hilo */
  int end;    /* Fin del rango para este hilo */
} thread_args_t;

/**
 * @brief Estructura para enviar mensajes entre procesos
 */
typedef struct {
  int round;    /* Número de rondas */
  int target;   /* Número a buscar */
  int solution; /* Mandar -1 al finalizalizar las rondas */
  int accepted; /* 1 si la solución es aceptada, 0 si no lo es */
} msg_t;

/**
 * @brief Estructura para registrar los tiempos de ejecución del programa
 */
typedef struct time_aa {
  int n_rounds;  /* Número de rondas */
  int n_threads; /* Número de threads */
  double time;   /* Tiempo medio de reloj */
} TIME_AA, *PTIME_AA;

/* Funciones privadas */

/**
 * @brief Se encarga de guardar en un archivo el resultado de las búsquedas
 *
 * @param read_fd Parte de la tubería sobre la que leer
 * @param write_fd Parte de la tubería sobre la que escribir
 * @return EXIT_FAILURE si hubo algún error, EXIT_SUCCESS si no
 */
int childLogger(int read_fd, int write_fd);

/**
 * @brief Minero completo: hace n_rounds rondas, y cada ronda el siguiente target pasa a ser la solución encontrada.
 *
 * @param target Número a buscar
 * @param n_rounds Número de rondas a realizar
 * @param n_threads Número de hilos a emplear
 * @param write_fd Parte de la tubería sobre la que escribir
 * @return EXIT_FAILURE si hubo algún error, EXIT_SUCCESS si no
 */
int parentMiner(int target, int n_rounds, int n_threads, int write_fd, int read_fd, TIME_AA* time);

/**
 * @brief Función privada que resuelve una ronda usando un cierto número hilos.
 *
 * @param target el target a buscar
 * @param n_threads el número de hilos a emplear
 * @return la solución encontrada, o -1 si no se encontró.
 */
int minerRound(int target, int n_threads);

/**
 * @brief Función que ejecutan los hilos, se ponen a probar los valores posibles dentro del rango que se les asignó y si
 * encuentran la solución, marcan found=1 y guardan la solución en solution
 *
 * @param arg puntero para una estructura que guarde  argumentos entre hilos
 * @return en nuestro caso siempre devuleve NULL pero es buenas prácticas en hilos que sean de tipo void*
 */
static void* minerWorker(void* arg);

/* Variables globales */
int fd_ml[2];      /* fd[0] para leer, fd[1] para escribir, ml(miner --> logger) */
int fd_lm[2];      /* fd[0] para leer, fd[1] para escribir, lm(logger --> miner) */
int found = 0;     /* Variable de control para que los hilos sepan si ya se ha encontrado la solución */
int solution = -1; /* Variable para guardar la solución encontrada por los hilos */

int main(int argc, char* argv[]) {
  int target, n_rounds, n_threads, res = 0;
  pid_t pid;
  TIME_AA time;
  FILE* f = NULL;

  /* CdE */
  if (argc != 4) {
    return -1;
  }

  /* Extraemos las variables de la línea de argumentos */
  target = atoi(argv[1]);
  n_rounds = atoi(argv[2]);
  n_threads = atoi(argv[3]);

  /* Pipe Miner->Logger */
  if (pipe(fd_ml) == -1) {
    perror("pipe fd_ml");
    exit(EXIT_FAILURE);
  }

  /* Pipe Logger->Miner */
  if (pipe(fd_lm) == -1) {
    perror("pipe fd_lm");
    exit(EXIT_FAILURE);
  }

  /* Creación de un nuevo proceso */
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) {
    /* Codigo del hijo (logger) */
    close(fd_ml[1]); /* No escribe en Miner->Logger */
    close(fd_lm[0]); /* No lee en Logger->Miner */

    res = childLogger(fd_ml[0], fd_lm[1]);

    close(fd_ml[0]);
    close(fd_lm[1]);
    exit(res);

  } else {
    /* Codigo del padre (miner) */
    close(fd_ml[0]); /* No lee en Miner->Logger */
    close(fd_lm[1]); /* No escribe en Logger->Miner */

    res = parentMiner(target, n_rounds, n_threads, fd_ml[1], fd_lm[0], &time);

    close(fd_ml[1]); /* Importante: EOF al logger cuando acabe */
    close(fd_lm[0]);

    if (res != EXIT_SUCCESS) {
      fprintf(stderr, "Error: parentMiner failed\n");
      exit(EXIT_FAILURE);
    }
  }

  /* Registro de los tiempos */
  f = fopen("tiempos.data", "a");
  if (!f) {
    return -1;
  }

  fprintf(f, "%i\t%i\t%i\t%f\n", target, time.n_rounds, time.n_threads, time.time);

  fclose(f);

  wait(NULL);
  exit(EXIT_SUCCESS);

  return 0;
}

/* Funciones privadas */

/**
 * @brief Se encarga de guardar en un archivo el resultado de las búsquedas
 */
int childLogger(int read_fd, int write_fd) {
  pid_t ppid = getppid();
  msg_t m;
  char filename[64];
  int out = 0;
  int ok = 1;
  ssize_t n = 0;

  /* Guardar nombre del fichero a crear en filename */
  snprintf(filename, sizeof(filename), "%jd.log", (intmax_t)ppid);

  /* Abrir el fichero para escribir, crear si no existe, vaciarlo si existe */
  out = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (out == -1) {
    perror("Error abriendo el fichero");
    printf("Logger exited unexpectedly\n");
    return EXIT_FAILURE;
  }

  while (1) {
    /* Logger recibe información de Miner a través de la tubería */
    n = read(read_fd, &m, sizeof(m));

    if (n == 0) {
      /* EOF: el padre cerró el pipe */
      break;
    }

    /* No se puedo leer la tubería */
    if (n == -1) {
      perror("read(pipe)");
      close(out);
      printf("Logger exited unexpectedly\n");
      return EXIT_FAILURE;
    }

    /* Si no se ha recibido el mensaje completo */
    if (n != (ssize_t)sizeof(m)) {
      fprintf(stderr, "Logger: mensaje incompleto leído (%zd bytes)\n", n);
      close(out);
      printf("Logger exited unexpectedly\n");
      return EXIT_FAILURE;
    }

    /* Fin marcado por el minero */
    if (m.solution == -1) {
      break;
    }

    if (m.accepted == 1) {
      dprintf(out,
              "Id:       %d\n"
              "Winner:   %jd\n"
              "Target:   %08d\n"
              "Solution: %d (validated)\n"
              "Votes:    %d/%d\n"
              "Wallets:  %jd:%d\n\n",
              m.round, (intmax_t)ppid, m.target, m.solution, m.round, m.round, (intmax_t)ppid, m.round);
    } else {
      dprintf(out,
              "Id:       %d\n"
              "Winner:   %jd\n"
              "Target:   %08d\n"
              "Solution: %d (rejected)\n"
              "Votes:    %d/%d\n"
              "Wallets:  %jd:%d\n\n",
              m.round, (intmax_t)ppid, m.target, m.solution, m.round, m.round, (intmax_t)ppid, m.round);
    }

    /* Enviar OK al minero para que continúe */
    if (write(write_fd, &ok, sizeof(ok)) != (ssize_t)sizeof(ok)) {
      perror("Error escribiendo en la tubería l->m");
      close(out);
      printf("Logger exited unexpectedly\n");
      return EXIT_FAILURE;
    }
  }

  close(out);

  printf("Logger exited with status %d\n", EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

/**
 * @brief Minero completo: hace n_rounds rondas, y cada ronda el siguiente target pasa a ser la solución encontrada.
 */
int parentMiner(int target, int n_rounds, int n_threads, int write_fd, int read_fd, TIME_AA* time) {
  int sol, ok, random = 0;
  int r = 1;
  clock_t start, end;
  ssize_t n = 0;
  msg_t m;

  start = clock();

  for (r = 1; r <= n_rounds; r++) {
    sol = minerRound(target, n_threads);

    /* Registar tiempos */
    end = clock();
    time->n_rounds = n_rounds;
    time->n_threads = n_threads;
    time->time = (end - start) / CLOCKS_PER_SEC;

    if (sol < 0) {
      fprintf(stderr, "Error: no se encontró solución en ronda %d\n", r);
      printf("Miner exited unexpectedly");
      return EXIT_FAILURE;
    }
    /* Para comprobar el apartado b) */
    printf("Solution accepted: %08d --> %d\n", target, sol);

    m.round = r;
    m.target = target;
    m.solution = sol;

    random = (rand() % 10) + 1;
    if (random > 9) {
      m.accepted = 0;
    } else {
      m.accepted = 1;
    }

    /* Enviar el mensaje con la solución encontrada */
    if (write(write_fd, &m, sizeof(m)) == -1) {
      perror("write(pipe)");
      printf("Miner exited unexpectedly");
      return EXIT_FAILURE;
    }

    /* Esperar al OK del logger */
    n = read(read_fd, &ok, sizeof(ok));

    if (n == 0) {
      fprintf(stderr, "Miner: logger cerró la pipe antes de tiempo\n");
      printf("Miner exited unexpectedly");
      return EXIT_FAILURE;
    }

    if (n != (ssize_t)sizeof(ok)) {
      perror("Error leyendo en la tubería l->m");
      printf("Miner exited unexpectedly");
      return EXIT_FAILURE;
    }

    if (ok != 1) {
      fprintf(stderr, "Error leyendo en la tubería l->m, ok=(%d)\n", ok);
      printf("Miner exited unexpectedly");
      return EXIT_FAILURE;
    }

    /* Siguiente ronda */
    target = sol; /* Siguiente objetivo para la siguiente ronda = solución actual */
    sol = -1;     /* Reiniciar variables de control para la siguiente ronda */
    found = 0;    /* Reiniciar variables de control para la siguiente ronda */
  }

  /* Enviar mensaje de finalización */
  m.round = 0;
  m.target = -1;
  m.solution = -1;
  m.accepted = 0;

  if (write(write_fd, &m, sizeof(m)) == -1) {
    perror("write(pipe)");
    printf("Miner exited unexpectedly\n");
    return EXIT_FAILURE;
  }

  printf("Miner exited with status %d\n", EXIT_SUCCESS);
  return EXIT_SUCCESS;
}

/**
 * @brief Función privada que resuelve una ronda usando un cierto número hilos.
 */
int minerRound(int target, int n_threads) {
  int i, j, chunk, rem, start, end, extra, err = 0;
  pthread_t* threads = NULL;
  thread_args_t* args = NULL;

  /* CdE */
  if (n_threads <= 0) {
    return -1;
  }

  /* Reservar memoria */
  threads = malloc(sizeof(pthread_t) * (size_t)n_threads);
  args = malloc(sizeof(thread_args_t) * (size_t)n_threads);
  if (!threads || !args) {
    free(threads);
    free(args);
    return -1;
  }

  /* División del espacio [0, POW_LIMIT) entre n_threads */
  chunk = POW_LIMIT / n_threads;
  rem = POW_LIMIT % n_threads;

  start = 0;

  /* Para cada hilo, calcular su rango de trabajo */
  for (i = 0; i < n_threads; i++) {
    extra = (i < rem) ? 1 : 0;
    end = start + chunk + extra;

    args[i].target = target;
    args[i].start = start;
    args[i].end = end;

    /* Crear el hilo y ponerlo a trabajar en su rango */
    err = pthread_create(&threads[i], NULL, minerWorker, &args[i]);

    /* Si falla crear un hilo: marcamos found para frenar a los ya creados */
    if (err != 0) {
      found = 1;

      /* Esperamos a los que sí se crearon */
      for (j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }

      free(threads);
      free(args);
      return -1;
    }

    start = end;
  }

  /* Esperamos a todos (cuando found=1, los demás saldrán solos) */
  for (i = 0; i < n_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  free(threads);
  free(args);
  return solution; /* Solution se actualiza por el hilo que encuentra la solución, o se queda en -1 si no se encontró */
}

/**
 * @brief Función que ejecutan los hilos, se ponen a probar los valores posibles dentro del rango que se les asignó y si
 * encuentran la solución, marcan found=1 y guardan la solución en solution
 */
static void* minerWorker(void* arg) {
  thread_args_t* a = (thread_args_t*)arg;
  int x = 0;

  for (x = a->start; x < a->end; x++) {
    /* Si otro hilo ya encontró la solución, sale */
    if (found == 1) {
      return NULL;
    }

    /* Calcula hash y compara con target */
    if (pow_hash(x) == a->target) {
      /* Marcar que ha encontrado solucion marcando found=1 */
      found = 1;
      solution = x;
      return NULL;
    }
  }

  return NULL;
}