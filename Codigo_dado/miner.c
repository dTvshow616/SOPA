#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "pow.h"

/**
 * Estructuras para pasar argumentos a los hilos y para enviar mensajes entre procesos
 */
typedef struct {
  int target; /*Objetivo a alcanzar*/
  int start;
  int end;              /*Inicio y fin del rango para este hilo*/
  atomic_int* found;    /*Flag para indicar si se encontró solución*/
  atomic_int* solution; /*Solución encontrada por este hilo*/
} thread_args_t;

typedef struct {
  int round;
  int target;
  int solution; /*Mandar -1 al finalizalizar las rondas*/
  int accepted; /*1 si la solución es aceptada, 0 si no lo es*/
} msg_t;

/**
 * Funciones privadas
 */
int miner_round(int target, int n_threads);
static void* mine_worker(void* arg);
int parentMiner(int target, int n_rounds, int n_threads, int write_fd);
int childLogger(int read_fd);

/*Variables globales*/
int fd[2]; /*fd[0] para leer, fd[1] para escribir*/

int main(int argc, char* argv[]) {
  int target, n_rounds, n_threads, res = 0;
  pid_t pid;

  if (argc != 4) {
    return -1;
  }

  target = atoi(argv[1]);
  n_rounds = atoi(argv[2]);
  n_threads = atoi(argv[3]);

  if (pipe(fd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  /*Creamos el fork*/
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) {
    /*Ejecutar el programa del hijo: Logger*/
    close(fd[1]); /*Cerrar la escritura en el hijo*/
    res = childLogger(fd[0]);
    close(fd[0]);
    exit(res);

  } else if (pid > 0) {
    /*Ejecutar el programa del padre: Minero*/
    close(fd[0]); /*Cerrar la lectura en el padre*/
    res = parentMiner(target, n_rounds, n_threads, fd[1]);
    close(fd[1]); /*Para que el hijo vea EOF cuando acaben rondas*/

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
int parentMiner(int target, int n_rounds, int n_threads, int write_fd) {
  msg_t m;
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

    m.round = r;
    m.target = target;
    m.solution = sol;
    m.accepted = 1; /*Accepted siempre por ahora*/

    /* Enviar el mensaje con la solución encontrada */
    if (write(write_fd, &m, sizeof(m)) == -1) {
      perror("write(pipe)");
      return EXIT_FAILURE;
    }

    target = sol; /*siguiente objetivo para la siguiente ronda = solución actual*/
  }

  /*Enviar mensaje de finalización*/
  m.round = 0;
  m.target = -1;
  m.solution = -1;
  m.accepted = 0;
  if (write(write_fd, &m, sizeof(m)) == -1) {
    perror("write(pipe)");
    return EXIT_FAILURE;
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

int childLogger(int read_fd) {
  pid_t ppid = getppid();
  msg_t m;
  char filename[64];
  int out = 0;
  ssize_t n = 0;

  /*Guardar nombre del fichero a crear en filename*/
  snprintf(filename, sizeof(filename), "%jd.log", (intmax_t)ppid);

  /*Abrir el fichero para escribir, crear si no existe, vaciarlo si existe*/
  out = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (out == -1) {
    perror("open(log)");
    return EXIT_FAILURE;
  }

  while (1) {
    n = read(read_fd, &m, sizeof(m));

    if (n == 0) {
      /* EOF: el padre cerró el pipe */
      break;
    }

    if (n == -1) {
      perror("read(pipe)");
      close(out);
      return EXIT_FAILURE;
    }

    /*Si no se ha recibido el mensaje completo*/
    if (n != (ssize_t)sizeof(m)) {
      fprintf(stderr, "Logger: mensaje incompleto leído (%zd bytes)\n", n);
      close(out);
      return EXIT_FAILURE;
    }

    /* Si usas el “fin” explícito */
    if (m.solution == -1) {
      break;
    }

    /* Formato del log (validated por defecto) */
    dprintf(out,
            "Id : %d\n"
            "Winner : %jd\n"
            "Target : %08d\n"
            "Solution : %d ( validated )\n"
            "Votes : %d/%d\n"
            "Wallets : %jd:%d\n",
            m.round, (intmax_t)ppid, m.target, m.solution, m.round, m.round, (intmax_t)ppid, m.round);
  }

  close(out);
  return EXIT_SUCCESS;
}
