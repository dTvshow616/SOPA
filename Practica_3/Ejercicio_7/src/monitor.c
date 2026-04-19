/**
 * @file monitor.c
 * @author Carlos Mendez & Ana Olsson
 * @brief Miner Rush program
 * @version 1.1
 * @date 2026-04-19
 */

#include "miner_rush.h" /* Librería que define variables y librerías globales para todos los procesos */

int main(int argc, char* argv[]) {
  /* Retraso en milisegundoes de cada cosa */
  int lag_comprobador, lag_monitor;

  if (argc != 3) {
    return -1;
  }

  lag_comprobador = atoi(argv[1]);
  lag_monitor = atoi(argv[2]);
  if (lag_comprobador < 0 || lag_monitor < 0) {
    perror("Invalid arguments\n");
    return -1;
  }

  comprobador(); /* Padre */

  return 0;
}

/**
 * @brief Se encarga de recibir los bloques por cola de mensajes y validarlos, padre de Monitor
 *
 */
void comprobador() {
  int pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    monitor();
  } else {
    /* Debe crear la cola de mensajes por donde le llegarán las soluciones a validar */
    // TODO Código
  }
}

/**
 * @brief Se encarga de mostrar la salida unificada, hijo de Comprobador
 *
 */
void monitor() {
  int fd_shm;         /* Descriptor del fichero */
  int* mapped = NULL; /* Puntero al segmento de memoria compartida */
  /* Arranca en primer lugar y finaliza el último */
  /* Debe crear los segmentos de memoria compartida y los semáforos que compartirán los procesos del sistema */
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

  Shared_Memory* shm = mmap(NULL, sizeof(Shared_Memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  shm->n_miners = 0;
  sem_init(&shm->miners_semaphore, 1, 1); /* __pshared = 1 para que se comparta entre procesos */
  if (!shm->miners_semaphore) {
    perror("sem_init");
    exit(EXIT_FAILURE);
  }

  // TODO: Meter el close(fd_shm);
  // TODO: Meter el shm_unlink(SHM_NAME);
  // TODO: Meter el sem_close(sem);
  // TODO: Meter el sem_unlink(SEM_NAME);
  /* Si este proceso se para, el sistema detendrá la ejecución del Comprobador (?) */
  /* Si un minero arranca antes que este proceso, dará un mensaje de error y saldrá del sistema */
}