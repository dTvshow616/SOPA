/**
 * @file monitor.c
 * @author Carlos Mendez & Ana Olsson
 * @brief Miner Rush program
 * @version 1.1
 * @date 2026-04-19
 */

#include "miner_rush.h" /* Librería que define variables y librerías globales para todos los procesos */

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

/**
 * @brief Se encarga de mostrar la salida unificada, hijo de Comprobador
 */
void monitor(int lag_monitor, int fd_shm, sem_t* sem);

/**
 * @brief Se encarga de recibir los bloques por cola de mensajes y validarlos, padre de Monitor
 */
void comprobador(int lag_comprobador, mqd_t queue);

/**
 * @brief It initializes the shared memory's info
 *
 * @param shm a pointer to the shared memory
 */
void init_shm(Shared_Memory* shm);

/* ----------------------------------------------------- Código ------------------------------------------------------ */

int main(int argc, char* argv[]) {
  int fd_shm;                       /* Descriptor del fichero de memoria compartida */
  sem_t* sem;                       /* Semáforo de los mineros */
  mqd_t queue;                      /* La cola de mensajes */
  int lag_comprobador, lag_monitor; /* Retraso en milisegundos de cada cosa */

  if (argc != 3) {
    return -1;
  }

  lag_comprobador = atoi(argv[1]);
  lag_monitor = atoi(argv[2]);
  if (lag_comprobador < 0 || lag_monitor < 0) {
    perror("Invalid arguments\n");
    return -1;
  }

  /* Mitosis */
  int pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);

  } else if (pid == 0) { /* Monitor */
    monitor(lag_monitor, fd_shm, sem);

  } else { /* Comprobador */
    comprobador(lag_comprobador, queue);
  }

  /* Ser buen padre */
  wait(NULL);

  /* Cerrar y borrar la memoria compartida */
  close(fd_shm);
  shm_unlink(SHM_NAME);

  /* Cerrar y borrar  el semáforo de los mineros */
  sem_close(sem);
  sem_unlink(SEM_NAME);

  /* Cerrar y borrar la cola de mensajes */
  mq_close(queue);
  mq_unlink(MQ_NAME);

  exit(EXIT_SUCCESS);

  return 0;
}

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

/**
 * @brief Se encarga de mostrar la salida unificada, hijo de Comprobador
 */
void monitor(int lag_monitor, int fd_shm, sem_t* sem) { /* REVIEW Arranca en primer lugar y finaliza el último */
  int* mapped = NULL;                                   /* Puntero al segmento de memoria compartida */
  Shared_Memory* shm;                                   /* Puntero a la memoria compartida*/

  /* Crear los segmentos de memoria compartida que compartirán los procesos del sistema */
  fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fd_shm == -1) {
    if (errno == EEXIST) {
      fd_shm = shm_open(SHM_NAME, O_RDWR, 0);
      if (fd_shm == -1) {
        perror("Error opening the shared memory segment");
        exit(EXIT_FAILURE);
      } else {
        printf("Shared memory segment open\n");
      }
    } else {
      perror("Error creating the shared memory segment\n");
      exit(EXIT_FAILURE);
    }
  } else {
    if (ftruncate(fd_shm, sizeof(Shared_Memory)) == -1) {
      perror("ftruncate");
      close(fd_shm);
      exit(EXIT_FAILURE);
    }
    printf("Shared memory segment created\n");
  }

  shm = mmap(NULL, sizeof(Shared_Memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  init_shm(shm);

  /* Crear los semáforos compartida que compartirán los procesos del sistema */
  if ((sem = (sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0))) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  // TOD: EL resto de cosillas

  /* TODO Si este proceso se para, el sistema detendrá la ejecución del Comprobador (?) */
  /* TODO Si un minero arranca antes que este proceso, dará un mensaje de error y saldrá del sistema */
}

/**
 * @brief Se encarga de recibir los bloques por cola de mensajes y validarlos, padre de Monitor
 */
void comprobador(int lag_comprobador, mqd_t queue) {
  Minero_Comprobador msg;

  /* Crear la cola de mensajes por donde le llegarán las soluciones a validar */
  queue = mq_open(MQ_NAME, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attributes);
  if (queue == (mqd_t)-1) {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }

  while (1) {
    /* Recibir mensajes de la cola */
    if (mq_receive(queue, (char*)&msg, sizeof(msg), NULL) == -1) {
      perror("mq_receive");
      exit(EXIT_FAILURE);
    }

    if (msg.is_last == 1) {
      /* TODO Introducir bloque de fin en memoria compartida para el Monitor */
      /* ... código del Productor ... */
      break;
    }

    /* TODO 3. Comprobar validez, introducir en memoria compartida (Productor) */
    /* ... código del Productor ... */

    /* Esperar el lag */
    usleep(lag_comprobador * 1000);
  }
}

/**
 * @brief It initializes the shared memory's info
 *
 * @param shm a pointer to the shared memory
 */
void init_shm(Shared_Memory* shm) {
  shm->n_miners = 0;
  shm->n_participants = 0;
  shm->n_votes = 0;
  shm->target = 0;
  shm->winner = -1;
  shm->winner_solution = -1;
}