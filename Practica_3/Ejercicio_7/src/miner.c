/**
 * @file miner.c
 * @author Carlos Mendez & Ana Olsson
 * @brief Miner Rush program
 * @version 5.1
 * @date 2026-04-19
 */

#include "miner_rush.h" /* Librería que define variables y librerías globales para todos los procesos */
#include "pow.h"        /* Librería interna para el POW */

#define TARGET_FILE "target.txt" /* El archivo que contiene el target a buscar */
#define VOTES_FILE "votes.txt"   /* El archivo que contiene los votos */
#define WINNER_FILE "winner.txt" /* El archivo que contiene el ganador */
#define MINERS_FILE "miners.txt" /* Fichero que comparten los mineros para guardar los mineros que están participando */
#define PARTICIPANTS_FILE                                                                                              \
  "participants.txt" /* Fichero que comparten los mineros para guardar el número de participantes activos en la ronda \
                        actual*/

/* --------------------------------------------------- Estructuras --------------------------------------------------- */

/**
 * @brief Estructura para pasar argumentos a los hilos (Miner -> mini_miners)
 */
typedef struct {
  int target; /* Objetivo a alcanzar */
  int start;  /* Inicio del rango para este hilo */
  int end;    /* Fin del rango para este hilo */
} thread_args_t;

/**
 * @brief Estructura para enviar mensajes entre los procesos Miner y Logger
 */
typedef struct {
  int round;    /* Número de ronda*/
  int target;   /* Objetivo a alcanzar */
  int solution; /* Solución encontrada */
  int accepted; /* 1 si la solución es aceptada, 0 si no lo es */
  int yes;      /* Número de votos afirmativos */
  int no;       /* Número de votos negativos */
  int coins;    /* Número de monedas ganadas */
} msg_t;

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

/**
 * @brief Se encarga de guardar en un archivo el resultado de las búsquedas
 *
 * @param read_fd Parte de la tubería sobre la que leer
 * @param write_fd Parte de la tubería sobre la que escribir
 * @return EXIT_FAILURE si hubo algún error, EXIT_SUCCESS si no
 */
int childLogger(int read_fd, int write_fd);

/**
 * @brief Minero completo: hace n_secs rondas, y tras cada ronda el siguiente target pasa a ser la solución encontrada.
 *
 * @param target Número a buscar
 * @param n_secs Tiempo a estar minando
 * @param n_threads Número de hilos a emplear
 * @param write_fd Parte de la tubería sobre la que escribir
 * @return EXIT_FAILURE si hubo algún error, EXIT_SUCCESS si no
 */
int parentMiner(int target, int n_secs, int n_threads, int write_fd, int read_fd);

/**
 * @brief Función privada que resuelve una ronda usando un cierto número de hilos.
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
 * @param arg puntero para una estructura que guarde argumentos entre hilos
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

/**
 * @brief Accede a la región de memoria compartida llamada SHM_NAME que es de tipo Shared_Memory
 */
void access_shared_memory();

/* ---------------------------------------------- Funciones auxiliares ----------------------------------------------- */

/**
 * @brief Funcion para configurar los handlers de las señales SIGALRM, SIGUSR1 y SIGUSR2
 */
static void setup_signal_handlers(void);

/**
 * @brief Función para inicializar las máscaras de señales usadas para bloquear y esperar señales en el minero
 */
static void init_signal_masks(void);

/**
 * @brief Función para manejar la señal de alarma.
 */
static void handler_sigalrm(int sig);

/**
 * @brief Función para manejar la señal de inicio de ronda.
 */
static void handler_sigusr1(int sig);

/**
 * @brief Función para manejar la señal de inicio de votación.
 */
static void handler_sigusr2(int sig);

/**
 * @brief Función para abrir el semáforo para manejar el acceso a los ficheros comunes
 * @return el semáforo abierto, o NULL si hubo un error
 */
static sem_t* open_mutex(void);

/**
 * @brief Función para limpiar un archivo de texto
 * @param path la ruta del archivo a limpiar
 */
static void clear_text_file(const char* path);

/**
 * @brief Función para escribir el target a buscar en el archivo de targets
 * @param target el target a escribir
 */
static void write_target_file(int target);

/**
 * @brief Función para leer el target a buscar del archivo de targets
 * @param target puntero al entero donde guardar el target leído
 * @return 1 si se leyó el target correctamente, 0 si no
 */
static int read_target_file(int* target);

/**
 * @brief Función para contar el número de mineros actuales en el sistema
 * @return el número de mineros actuales
 */
static int count_current_miners(void);

/**
 * @brief Función para mandar la señal indicada a los mineros del sistema
 * @param sig la señal a mandar
 */
static void broadcast_signal_to_miners(int sig);

/**
 * @brief Función para declararse winner de la ronda
 * @param pid Pid del proceso que ha ganado
 * @param solution Solucion encontrada para la ronda actual
 *
 * @return 1 si se declaró ganador correctamente, 0 si no
 */
static int claim_winner(pid_t pid, int solution);

/**
 * @brief Función para limpiar el archivo de ganador
 */
static void clear_winner_file(void);

/**
 * @brief Funcion para añadir un voto al archivo de votos
 * @param pid Pid del proceso que vota
 * @param vote El voto que hace el proceso (Y yes, N no)
 */
static void append_vote(pid_t pid, char vote);

/**
 * @brief Función para contar los votos en un archivo
 * @param yes puntero al entero donde guardar el número de votos positivos
 * @param no puntero al entero donde guardar el número de votos negativos
 * @param votes_str puntero a la cadena donde guardar los votos leídos
 * @param votes_str_size tamaño de la cadena de votos
 */
static void count_votes(int* yes, int* no, char* votes_str, size_t votes_str_size);

/**
 * @brief Función para esperar la señal SIGUSR1
 */
static void wait_for_usr1(void);

/**
 * @brief Función para esperar la señal SIGUSR2
 */
static void wait_for_usr2(void);

/**
 * @brief Función para limpiar el archivo de participantes activos en la ronda actual
 */
static void clear_participants_file(void);

/**
 * @brief Funcion para añadir un proceso al archivo de participantes de la ronda
 * @param pid Pid del proceso a añadir al archivo
 */
static void append_participant(pid_t pid);

/**
 * @brief Función para contar el número de participantes activos en la ronda actual
 * @return el número de participantes activos en la ronda actual
 */
static int count_participants(void);

/**
 * @brief Función para mandar una señal a todos los participantes de la ronda actual
 * @param sig la señal a mandar
 */
static void broadcast_signal_to_participants(int sig);

/* ----------------------------------------------- Variables globales ------------------------------------------------ */

int fd_ml[2]; /* fd[0] para leer, fd[1] para escribir, ml(miner --> logger) */
int fd_lm[2]; /* fd[0] para leer, fd[1] para escribir, lm(logger --> miner) */

int found = 0;     /* Variable de control para que los hilos sepan si ya se ha encontrado la solución */
int solution = -1; /* Variable para guardar la solución encontrada por los hilos */

volatile sig_atomic_t stop = 0;     /* Variable de control para que el minero sepa que se tiene que  parar */
volatile sig_atomic_t got_usr1 = 0; /* Variable de control para que el minero sepa que se ha recibido SIGUSR1 */
volatile sig_atomic_t got_usr2 = 0; /* Variable de control para que el minero sepa que se ha recibido SIGUSR2 */

static sigset_t blocked_set;    /* Máscara de señales bloqueadas durante la ejecución normal del minero */
static sigset_t wait_usr1_mask; /* Máscara de señales para esperar SIGUSR1 */
static sigset_t wait_usr2_mask; /* Máscara de señales para esperar SIGUSR2 */

static Shared_Memory* shm = NULL; /* Puntero a la memoria compartida */

/* ----------------------------------------------------- Código ------------------------------------------------------ */

int main(int argc, char* argv[]) {
  int target = 0;
  int n_threads, res, n_secs = 0;
  int is_first = 0;
  pid_t pid;

  /* Parsear los argumentos de entrada */
  if (argc != 3) {
    return -1;
  }

  n_secs = atoi(argv[1]);
  n_threads = atoi(argv[2]);
  if (target < 0 || n_secs < 0 || n_threads < 0) {
    perror("Invalid arguments\n");
    exit(EXIT_FAILURE);
  }

  /* Crear pipe Miner->Logger */
  if (pipe(fd_ml) == -1) {
    perror("pipe fd_ml");
    exit(EXIT_FAILURE);
  }

  /* Crear pipe Logger->Miner */
  if (pipe(fd_lm) == -1) {
    perror("pipe fd_lm");
    exit(EXIT_FAILURE);
  }

  /* Mitosis de los procesos */
  pid = fork();
  if (pid < 0) { /* Error */
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) { /* Logger */
    close(fd_ml[1]);     /* Cerrar la escritura en pipe Miner->Logger */
    close(fd_lm[0]);     /* Cerrar la lectura en pipe Logger->Miner */

    res = childLogger(fd_ml[0], fd_lm[1]);

    close(fd_ml[0]); /* Cerrar la lectura en pipe Miner->Logger */
    close(fd_lm[1]); /* Cerrar la escritura en pipe Logger->Miner */
    exit(res);

  } else {           /* Miner */
    close(fd_ml[0]); /* Cerrar la lectura en pipe Miner->Logger */
    close(fd_lm[1]); /* Cerrar la escritura en pipe Logger->Miner */

    /* Bloquear la entrada de señales para que no se pierdan */
    init_signal_masks();

    if (pthread_sigmask(SIG_BLOCK, &blocked_set, NULL) != 0) {
      perror("pthread_sigmask");
      exit(EXIT_FAILURE);
    }

    /* Configurar los manejadores de señales */
    setup_signal_handlers();

    /* Acceder a la memoria compartida */
    access_shared_memory();

    /* Crear un minero y mirar si es el primero o no*/
    is_first = add_miner(getpid());
    if (is_first) {
      while (count_current_miners() < 2 && !stop) {
        got_usr1 = 0;
        wait_for_usr1();
      }

      if (!stop) {
        write_target_file(0);
        clear_text_file(VOTES_FILE);
        clear_text_file(WINNER_FILE);
        clear_participants_file();

        printf("Initial target created: 0\n");

        broadcast_signal_to_miners(SIGUSR1);
        got_usr1 = 1;
      }
    }

    /* Comenzar a minar */
    res = parentMiner(target, n_secs, n_threads, fd_ml[1], fd_lm[0]);

    close(fd_ml[1]); /* Cerrar la escritura en pipe Miner->Logger */
    close(fd_lm[0]); /* Cerrar la lectura en pipe Logger->Miner */

    if (res != EXIT_SUCCESS) {
      fprintf(stderr, "Error: parentMiner failed\n");
      exit(EXIT_FAILURE);
    }
  }

  wait(NULL);
  remove_miner(getpid());

  exit(EXIT_SUCCESS);

  return 0;
}

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

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
  snprintf(filename, sizeof(filename), "%jd.log", (intmax_t)ppid);  // TODO: Esto debería dejar de usarse

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
              m.round, (intmax_t)ppid, m.target, m.solution, m.yes, (m.yes + m.no), (intmax_t)ppid, m.coins);
    } else {
      dprintf(out,
              "Id:       %d\n"
              "Winner:   %jd\n"
              "Target:   %08d\n"
              "Solution: %d (rejected)\n"
              "Votes:    %d/%d\n"
              "Wallets:  %jd:%d\n\n",
              m.round, (intmax_t)ppid, m.target, m.solution, m.yes, (m.yes + m.no), (intmax_t)ppid, m.coins);
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
int parentMiner(int target, int n_secs, int n_threads, int write_fd, int read_fd) {
  int sol, ok, r = 0;
  int coins = 0;
  int current_target = target;
  int expected_participants = 0;
  int proposed = 0;
  char votes_str[256];
  ssize_t n = 0;
  msg_t m;

  alarm(n_secs);

  while (!stop) {
    /* Esperar a que haya al menos dos mineros*/
    if (count_current_miners() < 2) {
      got_usr1 = 0;
      wait_for_usr1();
      continue;
    }

    /* Esperar a señal de comenzar ronda */
    if (!got_usr1) {
      wait_for_usr1();
    }
    if (stop) {
      break;
    }
    got_usr1 = 0;

    /* Comprobar que sigue habiendo al menos dos mineros*/
    if (count_current_miners() < 2) {
      continue;
    }

    /* Apuntarse a la ronda actual */
    append_participant(getpid());

    /* Dar un pequeño margen para que todos los mineros de esta ronda se apunten */
    for (int w = 0; w < 5 && !stop; w++) {
      usleep(100000);
    }

    /* Fijar cuántos participantes reales tiene esta ronda */
    expected_participants = count_participants();

    /* Si no hay suficientes participantes se aborta la ronda*/
    if (count_current_miners() < 2 || expected_participants < 2) {
      got_usr1 = 0;
      continue;
    }

    if (!read_target_file(&current_target)) {
      current_target = 0;
    }

    got_usr2 = 0;
    found = 0;
    solution = -1;

    /* Comenzar a minar esta ronda */
    sol = minerRound(current_target, n_threads);

    if (count_current_miners() < 2 || expected_participants < 2) {
      got_usr1 = 0;
      continue;
    }

    if (stop) {
      break;
    }

    /* Si es el primer minero en encontrar una solución */
    if (sol >= 0 && claim_winner(getpid(), sol)) {
      int yes = 0, no = 0;
      int expected;

      write_target_file(sol);
      clear_text_file(VOTES_FILE);

      broadcast_signal_to_participants(SIGUSR2);

      append_vote(getpid(), (pow_hash(sol) == current_target) ? 'Y' : 'N');

      expected = expected_participants;
      if (expected < 2 || count_current_miners() < 2) {
        clear_participants_file();
        clear_winner_file();

        /* Si hay suficientes mineros para una nueva ronda se manda la señal para comenzar*/
        if (count_current_miners() >= 2) {
          broadcast_signal_to_miners(SIGUSR1);
          got_usr1 = 1;
        } else {
          got_usr1 = 0;
        }

        continue;
      }

      /* Contar votos*/
      for (int i = 0; i < MAX_VOTE_WAIT; ++i) {
        votes_str[0] = '\0';
        count_votes(&yes, &no, votes_str, sizeof(votes_str));
        if ((yes + no) >= expected) {
          break;
        }
        usleep(100000);
      }

      count_votes(&yes, &no, votes_str, sizeof(votes_str));

      /* Si ya solo queda uno, o no han llegado todos los votos esperados,
         esta ronda no se acepta ni se imprime */
      if (count_current_miners() < 2 || (yes + no) < expected) {
        clear_participants_file();
        clear_winner_file();

        if (count_current_miners() >= 2) {
          broadcast_signal_to_miners(SIGUSR1);
          got_usr1 = 1;
        } else {
          got_usr1 = 0;
        }

        continue;
      }

      /* Imprimir resultados */
      printf("Winner %d => [ %s] => %s\n", (int)getpid(), votes_str, (yes >= no) ? "\x1b[32mAccepted\x1b[0m" : "\x1b[31mRejected\x1b[0m");

      if (yes >= no) {
        coins++;
      }

      m.round = r;
      m.target = current_target;
      m.solution = sol;
      m.accepted = (yes >= no) ? 1 : 0;
      m.yes = yes;
      m.no = no;
      m.coins = coins;

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

      /* Resetear ficheros y variables para siguiente ronda */
      clear_participants_file();
      clear_winner_file();

      current_target = sol;
      target = sol;
      r++;

      if (count_current_miners() >= 2) {
        broadcast_signal_to_miners(SIGUSR1);
        got_usr1 = 1;
      } else {
        got_usr1 = 0;
      }

      continue;
    }

    wait_for_usr2();
    if (stop) {
      break;
    }
    got_usr2 = 0;

    proposed = current_target;
    int proposed_read = 0;
    for (int retry = 0; retry < 5; retry++) {
      if (read_target_file(&proposed)) {
        proposed_read = 1;
        break;
      }
      usleep(100000);
    }

    if (!proposed_read) {
      fprintf(stderr, "Warning: failed to read proposed solution from %s, using current target %d for vote\n", TARGET_FILE, current_target);
    }

    append_vote(getpid(), (pow_hash(proposed) == current_target) ? 'Y' : 'N');

    current_target = proposed;
    target = proposed;
  }

  /* Se acaba su tiempo de minería */
  m.round = 0;
  m.target = -1;
  m.solution = -1;
  m.accepted = 0;
  m.yes = 0;
  m.no = 0;
  m.coins = 0;

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
  int is_first = 0;
  int count_before = 0;

  /* Esperar para acceder a la información para los mineros */
  while (sem_wait(shm->miners_semaphore) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      exit(EXIT_FAILURE);
    }
  }

  /* Contar los mineros apuntados */
  count_before = shm->n_miners;
  if (count_before == 0) {
    is_first = 1;
  }

  /* Apuntarse a la ronda */
  shm->miners[shm->n_miners] = pid;
  shm->n_miners++;

  /* Prints :3 */
  printf("\x1b[34mMiner %d added to system\x1b[0m\n", pid);
  printf("Current miners: ");
  for (int i = 0; i < shm->n_miners; i++) {
    printf("%d ", shm->miners[i]);
  }
  printf("\n");

  /* Indicar que ya está libre la información para los mineros */
  sem_post(shm->miners_semaphore);

  /* Empezar la ronda si ya había uno, si hay más la ronda ya ha empezado */
  if (count_before == 1) {
    broadcast_signal_to_miners(SIGUSR1);
  }

  return is_first;
}

/**
 * @brief Elimina un minero del sistema
 */
void remove_miner(pid_t pid) {
  int empty = 0;

  while (sem_wait(shm->miners_semaphore) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      exit(EXIT_FAILURE);
    }
  }

  /* Borrar el PID del array compactando */
  for (int i = 0; i < shm->n_miners; i++) {
    if (shm->miners[i] == pid) {
      /* Mover el último elemento a esta posición */
      shm->miners[i] = shm->miners[shm->n_miners - 1];
      shm->n_miners--;
      break;
    }
  }

  printf("\x1b[35mMiner %d exited system\x1b[0m\n", pid);

  empty = (shm->n_miners == 0);

  if (!empty) {
    printf("Current miners: ");
    for (int i = 0; i < shm->n_miners; i++) {
      printf("%d ", shm->miners[i]);
    }
    printf("\n");
  }

  sem_post(shm->miners_semaphore);

  if (empty) {
    printf("No miners left.\n");
    /* TODO: Enviar bloque de finalización a la cola de mensajes aquí */
  }
}

/**
 * @brief Accede a la región de memoria compartida llamada SHM_NAME que es de tipo Shared_Memory
 */
void access_shared_memory() {
  /* 1. Obtener un descriptor de fichero a la memoria compartida con shm_open */
  int fd_shm = shm_open(SHM_NAME, O_RDWR, 0);
  if (fd_shm == -1) {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  /* 2. Enlazar la memoria al espacio de direcciones del proceso con mmap y cerrar el descriptor de fichero con close */
  shm = mmap(NULL, sizeof(Shared_Memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  close(fd_shm);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
  }
}

/* ---------------------------------------------- Funciones auxiliares ----------------------------------------------- */

/**
 * @brief Funcion para configurar los handlers de las señales SIGALRM, SIGUSR1 y SIGUSR2
 */
static void setup_signal_handlers(void) {
  struct sigaction act;

  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGUSR1);
  sigaddset(&act.sa_mask, SIGUSR2);
  sigaddset(&act.sa_mask, SIGALRM);
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

/**
 * @brief Función para inicializar las máscaras de señales usadas para bloquear y esperar señales en el minero
 */
static void init_signal_masks(void) {
  sigemptyset(&blocked_set);
  sigaddset(&blocked_set, SIGUSR1);
  sigaddset(&blocked_set, SIGUSR2);
  sigaddset(&blocked_set, SIGALRM);

  wait_usr1_mask = blocked_set;
  sigdelset(&wait_usr1_mask, SIGUSR1);
  sigdelset(&wait_usr1_mask, SIGALRM);

  wait_usr2_mask = blocked_set;
  sigdelset(&wait_usr2_mask, SIGUSR2);
  sigdelset(&wait_usr2_mask, SIGALRM);
}

/**
 * @brief Función para manejar la señal de alarma.
 */
static void handler_sigalrm(int sig) {
  (void)sig;
  stop = 1;
}

/**
 * @brief Función para manejar la señal de inicio de ronda.
 */
static void handler_sigusr1(int sig) {
  (void)sig;
  got_usr1 = 1;
}

/**
 * @brief Función para manejar la señal de inicio de votación.
 */
static void handler_sigusr2(int sig) {
  (void)sig;
  got_usr2 = 1;
}

/**
 * @brief Función para abrir el semáforo para manejar el acceso a los ficheros comunes
 */
static sem_t* open_mutex(void) {
  sem_t* sem = shm->miners_semaphore;
  if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  return sem;
}

/**
 * @brief Función para limpiar un archivo de texto
 */
static void clear_text_file(const char* path) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para escribir el target a buscar en el archivo de targets
 */
static void write_target_file(int target) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para leer el target a buscar del archivo de targets
 */
static int read_target_file(int* target) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int ok = 0;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para contar el número de mineros actuales en el sistema
 */
static int count_current_miners(void) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p, count = 0;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para mandar la señal indicada a los mineros del sistema
 */
static void broadcast_signal_to_miners(int sig) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para declararse winner de la ronda
 */
static int claim_winner(pid_t pid, int solution) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int old_pid = 0, old_solution = 0;
  int already_claimed = 0;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para limpiar el archivo de ganador
 */
static void clear_winner_file(void) { clear_text_file(WINNER_FILE); }

/**
 * @brief Funcion para añadir un voto al archivo de votos
 */
static void append_vote(pid_t pid, char vote) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para contar los votos en un archivo
 */
static void count_votes(int* yes, int* no, char* votes_str, size_t votes_str_size) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int pid;
  char vote;
  size_t used = 0;

  *yes = 0;
  *no = 0;

  if (votes_str && votes_str_size > 0) {
    votes_str[0] = '\0';
  }

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
  }

  f = fopen(VOTES_FILE, "r");
  if (f) {
    while (fscanf(f, "%d %c", &pid, &vote) == 2) {
      if (vote == 'Y') {
        (*yes)++;
      } else if (vote == 'N') {
        (*no)++;
      }

      if (votes_str && votes_str_size > 0) {
        int n = snprintf(votes_str + used, votes_str_size - used, "%c ", vote);
        if (n < 0 || (size_t)n >= votes_str_size - used) {
          break;
        }
        used += (size_t)n;
      }
    }
    fclose(f);
  }

  sem_post(sem);
  sem_close(sem);
}

/**
 * @brief Función para esperar la señal SIGUSR1
 */
static void wait_for_usr1(void) {
  while (!got_usr1 && !stop) {
    sigsuspend(&wait_usr1_mask);
  }
}

/**
 * @brief Función para esperar la señal SIGUSR2
 */
static void wait_for_usr2(void) {
  while (!got_usr2 && !stop) {
    sigsuspend(&wait_usr2_mask);
  }
}

/**
 * @brief Función para limpiar el archivo de participantes activos en la ronda actual
 */
static void clear_participants_file(void) { clear_text_file(PARTICIPANTS_FILE); }

/**
 * @brief Funcion para añadir un proceso al archivo de participantes de la ronda
 */
static void append_participant(pid_t pid) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para contar el número de participantes activos en la ronda actual
 */
static int count_participants(void) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p, count = 0;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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

/**
 * @brief Función para mandar una señal a todos los participantes de la ronda actual
 */
static void broadcast_signal_to_participants(int sig) {
  sem_t* sem = open_mutex();
  FILE* f = NULL;
  int p;

  while (sem_wait(sem) < 0) {
    if (errno != EINTR) {
      perror("sem_wait");
      sem_close(sem);
      exit(EXIT_FAILURE);
    }
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