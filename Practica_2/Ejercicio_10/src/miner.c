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
#include <semaphore.h>/* Semaphores to manage the different processes*/
#include <signal.h>   /* Signal handling */
#include <stdint.h>   /* exact-width integer types */
#include <stdio.h>    /* standard input/output library functions */
#include <stdlib.h> /* numeric conversion functions, pseudo-random numbers generation functions, memory allocation, process control functions */
#include <sys/wait.h> /* wait for process to change state */
#include <time.h>     /*Tiempitos*/
#include <unistd.h>   /* POSIX operating system API */

#include "pow.h" /* Librería interna para el POW */

#define TARGET_FILE "target.txt"  /* El archivo que contiene el target a buscar */
#define VOTES_FILE "votes.txt"    /* El archivo que contiene los votos */
#define WINNER_FILE "winner.txt"  /* El archivo que contiene el ganador */
#define MAX_VOTE_WAIT 50          /* Tiempo máximo a esperar por los votos de los mineros (en segundos) */
#define MINERS_FILE "miners.txt"  /* Fichero que comparten los mineros para guardar los mineros participando*/
#define SEM_MUTEX "/miners_mutex" /* Semáforo para gestionar el acceso al fichero de mineros */
#define PARTICIPANTS_FILE \
  "participants.txt" /* Fichero que comparten los mineros para guardar el número de participantes activos en la ronda actual*/
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
  int n_secs;    /* Número de segundos */
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
 * @brief Minero completo: hace n_secs rondas, y cada ronda el siguiente target pasa a ser la solución encontrada.
 *
 * @param target Número a buscar
 * @param n_secs Tiempo a estar minando
 * @param n_threads Número de hilos a emplear
 * @param write_fd Parte de la tubería sobre la que escribir
 * @return EXIT_FAILURE si hubo algún error, EXIT_SUCCESS si no
 */
int parentMiner(int target, int n_secs, int n_threads, int write_fd, int read_fd, TIME_AA* time);

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

/**
 * @brief Añade un nuevo minero al sistema
 * @param pid el PID del nuevo minero
 *
 * @return 1 si el minero añadido es el primero en el sistema, 0 si no lo es
 */
int add_miner(pid_t pid);

/**
 * @brief Elimina un minero del sistema
 * @param pid el PID del minero a eliminar
 */
void remove_miner(pid_t pid);

/**
 * @brief Handler para la señal SIGALRM, se encarga de marcar la variable stop para que el minero sepa que ha llegado el momento
 * de parar
 * @param sig el número de la señal recibida
 */
void handler(int sig);

/* Variables globales */
int fd_ml[2];                   /* fd[0] para leer, fd[1] para escribir, ml(miner --> logger) */
int fd_lm[2];                   /* fd[0] para leer, fd[1] para escribir, lm(logger --> miner) */
int found = 0;                  /* Variable de control para que los hilos sepan si ya se ha encontrado la solución */
int solution = -1;              /* Variable para guardar la solución encontrada por los hilos */
volatile sig_atomic_t stop = 0; /* Variable de control para que el minero sepa cuándo parar */
volatile sig_atomic_t got_usr1 = 0;
volatile sig_atomic_t got_usr2 = 0;

/* Funciones auxiliares */
static void setup_signal_handlers(void);
static void handler_sigalrm(int sig);
static void handler_sigusr1(int sig);
static void handler_sigusr2(int sig);

static sem_t* open_mutex(void);
static int count_current_miners(void);
static void write_target_file(int target);
static int read_target_file(int* target);
static void clear_text_file(const char* path);
static void broadcast_signal_to_miners(int sig);
static int claim_winner(pid_t pid, int solution);
static void clear_winner_file(void);
static void append_vote(pid_t pid, char vote);
static void count_votes(int* yes, int* no);
static void wait_for_usr1(void);
static void wait_for_usr2(void);
static void clear_participants_file(void);
static void append_participant(pid_t pid);
static int count_participants(void);
static void broadcast_signal_to_participants(int sig);

int main(int argc, char* argv[]) {
  int target = 0;
  int n_threads, res, n_secs = 0;
  pid_t pid;
  TIME_AA time;
  FILE* f = NULL;
  int is_first = 0;

  if (argc != 3) {
    return -1;
  }

  n_secs = atoi(argv[1]);
  n_threads = atoi(argv[2]);
  if (target < 0 || n_secs < 0 || n_threads < 0) {
    perror("Invalid arguments\n");
    exit(EXIT_FAILURE);
  }

  if (pipe(fd_ml) == -1) {
    perror("pipe fd_ml");
    exit(EXIT_FAILURE);
  }

  if (pipe(fd_lm) == -1) {
    perror("pipe fd_lm");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) {
    close(fd_ml[1]);
    close(fd_lm[0]);

    res = childLogger(fd_ml[0], fd_lm[1]);

    close(fd_ml[0]);
    close(fd_lm[1]);
    exit(res);

  } else {
    close(fd_ml[0]);
    close(fd_lm[1]);

    setup_signal_handlers();

    is_first = add_miner(getpid());

    if (is_first) {
      write_target_file(0);
      clear_text_file(VOTES_FILE);
      clear_text_file(WINNER_FILE);
      clear_participants_file();

      printf("Initial target created: 0\n");

      broadcast_signal_to_miners(SIGUSR1);
      got_usr1 = 1;
    }

    if (count_current_miners() == 1) {
      write_target_file(0);
      clear_text_file(VOTES_FILE);
      clear_text_file(WINNER_FILE);

      printf("Initial target created: 0\n");

      broadcast_signal_to_miners(SIGUSR1);
      got_usr1 = 1;
    }

    res = parentMiner(target, n_secs, n_threads, fd_ml[1], fd_lm[0], &time);

    close(fd_ml[1]);
    close(fd_lm[0]);

    if (res != EXIT_SUCCESS) {
      fprintf(stderr, "Error: parentMiner failed\n");
      exit(EXIT_FAILURE);
    }
  }

  f = fopen("tiempos.data", "a");
  if (!f) {
    return -1;
  }

  fprintf(f, "%i\t%i\t%i\t%f\n", target, time.n_secs, time.n_threads, time.time);
  fclose(f);

  wait(NULL);
  remove_miner(getpid());
  exit(EXIT_SUCCESS);

  return 0;
}

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
 * @brief Handler para la señal SIGALRM, se encarga de marcar la variable stop para que el minero sepa que ha llegado el momento
 * de parar
 */
void handler(int sig) {
  (void)sig;
  stop = 1;
}

/**
 * @brief Minero completo: hace n_secs rondas, y cada ronda el siguiente target pasa a ser la solución encontrada.
 */
int parentMiner(int target, int n_secs, int n_threads, int write_fd, int read_fd, TIME_AA* time) {
  int sol, ok, r = 0;
  clock_t start, end;
  ssize_t n = 0;
  msg_t m;
  int current_target = target;

  start = clock();

  signal(SIGALRM, handler_sigalrm);
  alarm(n_secs);

  while (!stop) {
    if (!got_usr1) {
      wait_for_usr1();
    }
    if (stop) {
      break;
    }
    got_usr1 = 0;
    append_participant(getpid());

    if (!read_target_file(&current_target)) {
      current_target = 0;
    }

    got_usr2 = 0;
    found = 0;
    solution = -1;

    sol = minerRound(current_target, n_threads);

    end = clock();
    time->n_secs = n_secs;
    time->n_threads = n_threads;
    time->time = (end - start) / CLOCKS_PER_SEC;

    if (stop) {
      break;
    }

    if (sol >= 0 && claim_winner(getpid(), sol)) {
      int yes = 0, no = 0;
      int expected;

      write_target_file(sol);
      clear_text_file(VOTES_FILE);

      broadcast_signal_to_participants(SIGUSR2);

      append_vote(getpid(), (pow_hash(sol) == current_target) ? 'Y' : 'N');

      expected = count_participants();

      for (int i = 0; i < MAX_VOTE_WAIT; ++i) {
        count_votes(&yes, &no);
        if ((yes + no) >= expected) {
          break;
        }
        usleep(100000);
      }

      count_votes(&yes, &no);

      printf("Winner %d => [ %d Y %d N ] => %s\n", (int)getpid(), yes, no, (yes >= no) ? "Accepted" : "Rejected");

      m.round = r;
      m.target = current_target;
      m.solution = sol;
      m.accepted = (yes >= no) ? 1 : 0;

      if (write(write_fd, &m, sizeof(m)) == -1) {
        perror("write(pipe)");
        printf("Miner exited unexpectedly\n");
        return EXIT_FAILURE;
      }

      n = read(read_fd, &ok, sizeof(ok));
      if (n == 0) {
        fprintf(stderr, "Miner: logger cerró la pipe antes de tiempo\n");
        printf("Miner exited unexpectedly\n");
        return EXIT_FAILURE;
      }

      if (n != (ssize_t)sizeof(ok)) {
        perror("Error leyendo en la tubería l->m");
        printf("Miner exited unexpectedly\n");
        return EXIT_FAILURE;
      }

      if (ok != 1) {
        fprintf(stderr, "Error leyendo en la tubería l->m, ok=(%d)\n", ok);
        printf("Miner exited unexpectedly\n");
        return EXIT_FAILURE;
      }

      clear_participants_file();
      broadcast_signal_to_miners(SIGUSR1);
      got_usr1 = 1;

      current_target = sol;
      target = sol;
      r++;
      clear_winner_file();

      broadcast_signal_to_miners(SIGUSR1);
      got_usr1 = 1;
      continue;
    }

    wait_for_usr2();
    if (stop) {
      break;
    }
    got_usr2 = 0;

    int proposed = 0;
    if (!read_target_file(&proposed)) {
      proposed = current_target;
    }

    append_vote(getpid(), (pow_hash(proposed) == current_target) ? 'Y' : 'N');

    wait_for_usr1();
    got_usr1 = 0;
    got_usr2 = 0;

    current_target = proposed;
    target = proposed;
  }

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
    if (found == 1 || got_usr2 == 1 || stop == 1) {
      return NULL;
    }

    if (pow_hash(x) == a->target) {
      found = 1;
      solution = x;
      return NULL;
    }
  }

  return NULL;
}

/**
 * @brief Añade un nuevo minero al sistema
 */
int add_miner(pid_t pid) {
  sem_t* sem;
  FILE* f;
  int is_first = 0;
  int tmp;

  sem = sem_open(SEM_MUTEX, O_CREAT, 0644, 1);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  sem_wait(sem);

  f = fopen(MINERS_FILE, "a+");
  if (!f) {
    perror("fopen");
    sem_post(sem);
    exit(EXIT_FAILURE);
  }

  /* Comprobar si estaba vacío ANTES de añadir */
  rewind(f);
  if (fscanf(f, "%d", &tmp) != 1) {
    is_first = 1;
  }

  /* Añadir este minero */
  fprintf(f, "%d\n", pid);
  fclose(f);

  printf("Miner %d added to system\n", pid);

  /* Mostrar todos */
  f = fopen(MINERS_FILE, "r");
  if (f) {
    int p;
    printf("Current miners: ");
    while (fscanf(f, "%d", &p) == 1) {
      printf("%d ", p);
    }
    printf("\n");
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);

  return is_first;
}

/**
 * @brief Elimina un minero del sistema
 */
void remove_miner(pid_t pid) {
  sem_t* sem;
  FILE *f, *temp;
  int p;
  int empty = 1;

  sem = sem_open(SEM_MUTEX, 0);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  sem_wait(sem);

  f = fopen(MINERS_FILE, "r");
  temp = fopen("temp.txt", "w");

  if (!f || !temp) {
    perror("fopen");
    sem_post(sem);
    exit(EXIT_FAILURE);
  }

  /* Copiar todos excepto el minero que se elimina */
  while (fscanf(f, "%d", &p) == 1) {
    if (p != pid) {
      fprintf(temp, "%d\n", p);
    }
  }

  fclose(f);
  fclose(temp);

  rename("temp.txt", MINERS_FILE);

  printf("Miner %d exited system\n", pid);

  /* Comprobar si el fichero está vacío */
  f = fopen(MINERS_FILE, "r");
  if (f) {
    if (fscanf(f, "%d", &p) == 1) {
      empty = 0;
    }
    fclose(f);
  }

  if (empty) {
    printf("No miners left. Cleaning system...\n");
    remove(MINERS_FILE);
    sem_unlink(SEM_MUTEX);
  } else {
    f = fopen(MINERS_FILE, "r");
    if (f) {
      printf("Current miners: ");
      while (fscanf(f, "%d", &p) == 1) {
        printf("%d ", p);
      }
      printf("\n");
      fclose(f);
    }
  }

  sem_post(sem);
  sem_close(sem);
}

/* Funciones auxiliares */

static void setup_signal_handlers(void) {
  struct sigaction act;

  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  act.sa_handler = handler_sigalrm;
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    perror("sigaction(SIGALRM)");
    exit(EXIT_FAILURE);
  }

  act.sa_handler = handler_sigusr1;
  if (sigaction(SIGUSR1, &act, NULL) < 0) {
    perror("sigaction(SIGUSR1)");
    exit(EXIT_FAILURE);
  }

  act.sa_handler = handler_sigusr2;
  if (sigaction(SIGUSR2, &act, NULL) < 0) {
    perror("sigaction(SIGUSR2)");
    exit(EXIT_FAILURE);
  }
}

static void handler_sigalrm(int sig) {
  (void)sig;
  stop = 1;
}

static void handler_sigusr1(int sig) {
  (void)sig;
  got_usr1 = 1;
}

static void handler_sigusr2(int sig) {
  (void)sig;
  got_usr2 = 1;
}

static sem_t* open_mutex(void) {
  sem_t* sem = sem_open(SEM_MUTEX, 0);
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  return sem;
}

static void clear_text_file(const char* path) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(path, "w");
  if (!f) {
    perror("fopen");
    sem_post(sem);
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  fclose(f);
  sem_post(sem);
  sem_close(sem);
}

static void write_target_file(int target) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(TARGET_FILE, "w");
  if (!f) {
    perror("fopen");
    sem_post(sem);
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  fprintf(f, "%d\n", target);
  fclose(f);

  sem_post(sem);
  sem_close(sem);
}

static int read_target_file(int* target) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int ok = 0;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(TARGET_FILE, "r");
  if (f) {
    if (fscanf(f, "%d", target) == 1) {
      ok = 1;
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
  return ok;
}

static int count_current_miners(void) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p, count = 0;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(MINERS_FILE, "r");
  if (f) {
    while (fscanf(f, "%d", &p) == 1) {
      count++;
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
  return count;
}

static void broadcast_signal_to_miners(int sig) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(MINERS_FILE, "r");
  if (f) {
    while (fscanf(f, "%d", &p) == 1) {
      if (p != (int)getpid()) {
        kill((pid_t)p, sig);
      }
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
}

static int claim_winner(pid_t pid, int solution) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int old_pid = 0, old_solution = 0;
  int already_claimed = 0;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(WINNER_FILE, "r");
  if (f) {
    if (fscanf(f, "%d %d", &old_pid, &old_solution) == 2) {
      already_claimed = 1;
    }
    fclose(f);
  }

  if (!already_claimed) {
    f = fopen(WINNER_FILE, "w");
    if (!f) {
      perror("fopen");
      sem_post(sem);
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
    fprintf(f, "%d %d\n", (int)pid, solution);
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
  return !already_claimed;
}

static void clear_winner_file(void) { clear_text_file(WINNER_FILE); }

static void append_vote(pid_t pid, char vote) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(VOTES_FILE, "a");
  if (!f) {
    perror("fopen");
    sem_post(sem);
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  fprintf(f, "%d %c\n", (int)pid, vote);
  fclose(f);

  sem_post(sem);
  sem_close(sem);
}

static void count_votes(int* yes, int* no) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int pid;
  char vote;

  *yes = 0;
  *no = 0;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(VOTES_FILE, "r");
  if (f) {
    while (fscanf(f, "%d %c", &pid, &vote) == 2) {
      if (vote == 'Y') {
        (*yes)++;
      } else if (vote == 'N') {
        (*no)++;
      }
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
}

static void wait_for_usr1(void) {
  sigset_t mask, oldmask;

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);

  /* Bloquear SIGUSR1 */
  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  while (!got_usr1 && !stop) {
    sigsuspend(&oldmask); /* espera con señales desbloqueadas */
  }

  /* Restaurar máscara */
  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static void wait_for_usr2(void) {
  sigset_t mask, oldmask;

  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR2);

  sigprocmask(SIG_BLOCK, &mask, &oldmask);

  while (!got_usr2 && !stop) {
    sigsuspend(&oldmask);
  }

  sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

static void clear_participants_file(void) { clear_text_file(PARTICIPANTS_FILE); }

static void append_participant(pid_t pid) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(PARTICIPANTS_FILE, "a");
  if (!f) {
    perror("fopen");
    sem_post(sem);
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  fprintf(f, "%d\n", (int)pid);
  fclose(f);

  sem_post(sem);
  sem_close(sem);
}

static int count_participants(void) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p, count = 0;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(PARTICIPANTS_FILE, "r");
  if (f) {
    while (fscanf(f, "%d", &p) == 1) {
      count++;
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
  return count;
}

static void broadcast_signal_to_participants(int sig) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p;

  if (sem_wait(sem) < 0) {
    perror("sem_wait");
    sem_close(sem);
    exit(EXIT_FAILURE);
  }

  f = fopen(PARTICIPANTS_FILE, "r");
  if (f) {
    while (fscanf(f, "%d", &p) == 1) {
      if (p != (int)getpid()) {
        kill((pid_t)p, sig);
      }
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
}
