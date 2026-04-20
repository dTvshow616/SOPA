/**
 * @file monitor.c
 * @author Carlos Mendez & Ana Olsson
 * @brief Miner Rush program
 * @version 2.1
 * @date 2026-04-20
 */

#include "miner_rush.h" /* Librería que define variables y librerías globales para todos los procesos */

/* --------------------------------------------------- Estructuras --------------------------------------------------- */

/**
 * @brief Atributos de la cola de mensajes entre Minero y Comprobador
 */
struct mq_attr attributes = {.mq_flags = 0, .mq_maxmsg = 7, .mq_curmsgs = 0, .mq_msgsize = sizeof(Minero_Comprobador)};

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

/**
 * @brief Se encarga de mostrar la salida unificada, hijo de Comprobador
 */
void monitor(int lag_monitor);

/**
 * @brief Se encarga de recibir los bloques por cola de mensajes y validarlos, padre de Monitor
 */
void comprobador(int lag_comprobador);

/**
 * @brief It initializes the shared memory's info
 *
 * @param shm a pointer to the shared memory
 */
void init_shm(Shared_Memory* shm);

/* ----------------------------------------------------- Código ------------------------------------------------------ */

int main(int argc, char* argv[]) {
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
    monitor(lag_monitor);

  } else { /* Comprobador */
    comprobador(lag_comprobador);

    /* Ser buen padre */
    wait(NULL);
  }

  return 0;
}

/* ----------------------------------------------- Funciones privadas ------------------------------------------------ */

/**
 * @brief Se encarga de mostrar la salida unificada, hijo de Comprobador
 */
void monitor(int lag_monitor) { /* NOTE Arranca en primer lugar y finaliza el último */
  int fd_shm;                   /* Descriptor de fichero de la memoria compartida*/
  Shared_Memory* shm;           /* Puntero a la memoria compartida*/
  sem_t* sem_miners;            /* Puntero al semáforo de los mineros */
  Bloque_Buffer bloque;         /* Bloque del Productor-Consumidor */

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
  close(fd_shm);

  init_shm(shm);

  /* Semáforos del Productor-Consumidor */
  sem_init(&shm->sem_empty, 1, 6); /* 6 huecos vacíos */
  sem_init(&shm->sem_fill, 1, 0);  /* 0 huecos llenos */
  sem_init(&shm->sem_mutex, 1, 1); /* Mutex a 1 */

  /* Crear los semáforos compartida que compartirán los procesos del sistema */
  if ((sem_miners = (sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1))) == SEM_FAILED) {
    sem_miners = sem_open(SEM_NAME, 0);
  }

  printf("[%d] Printing blocks...\n", getpid());
  while (1) {
    /* Modelo Consumidor */
    sem_wait(&shm->sem_fill);  /* Down(sem_fill); */
    sem_wait(&shm->sem_mutex); /* Down(sem_mutex); */
    /* ExtraerElemento(); */
    bloque = shm->buffer[shm->out]; /* Extraer último elemento */
    shm->out = (shm->out + 1) % 6;  /* Actualizar índice último elemento en buffer circular */
    sem_post(&shm->sem_mutex);      /* Up(sem_mutex); */
    sem_post(&shm->sem_empty);      /* Up(sem_empty); */

    /* Terminar si el programa ha terminado */
    if (bloque.is_last) {
      break;
    }

    /* Prints :3 */
    if (bloque.is_valid == 1) {
      printf("Solution \x1b[32maccepted\x1b[0m: %08ld --> %08ld\n", bloque.target, bloque.solution);
    } else {
      printf("Solution \x1b[31mrejected\x1b[0m: %08ld !-> %08ld\n", bloque.target, bloque.solution);
    }

    /* Esperar el lag */
    usleep(lag_monitor * 1000);
  }

  printf("\x1b[35m[%d] Finishing (Monitor)\x1b[0m\n", getpid());

  /* Destruir los semáforos del Productor-Consumidor */
  sem_destroy(&shm->sem_empty);
  sem_destroy(&shm->sem_fill);
  sem_destroy(&shm->sem_mutex);

  /* Liberar la memoria compartida */
  munmap(shm, sizeof(Shared_Memory));
  shm_unlink(SHM_NAME);

  /* Cerrar y borrar el semáforo de los mineros */
  sem_close(sem_miners);
  sem_unlink(SEM_NAME);

  exit(EXIT_SUCCESS);

  /* TODO Si este proceso se para, el sistema detendrá la ejecución del Comprobador (?) */
}

/**
 * @brief Se encarga de recibir los bloques por cola de mensajes y validarlos, padre de Monitor
 */
void comprobador(int lag_comprobador) {
  Minero_Comprobador msg;      /* El mensaje de la cola de mensajes */
  Shared_Memory* shm;          /* Puntero a la memoria compartida*/
  mqd_t queue;                 /* La cola de mensajes */
  int i, fd_shm, found_wallet; /* Variable de bucle, descriptor del fichero de memoria compartida y si se ha encontrado la cartera */
  Bloque_Buffer bloque;        /* Bloque que será añadido al buffer del modelo Productor-Consumidor */

  /* Dar mini retardo para que se cree la memoria compartida */
  usleep(100000);

  /* Acceder a la memoria compartida creada por Monitor */
  fd_shm = shm_open(SHM_NAME, O_RDWR, 0);
  shm = mmap(NULL, sizeof(Shared_Memory), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  close(fd_shm);

  /* Crear la cola de mensajes por donde le llegarán las soluciones a validar */
  queue = mq_open(MQ_NAME, O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR, &attributes);
  if (queue == (mqd_t)-1) {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }

  printf("[%d] Checking blocks...\n", getpid());
  while (1) {
    /* Recibir mensajes de la cola cada LAG_C */
    if (mq_receive(queue, (char*)&msg, sizeof(msg), NULL) == -1) {
      perror("mq_receive");
      exit(EXIT_FAILURE);
    }

    /* Guardar info recibida en el bloque */
    bloque.target = msg.target;
    bloque.solution = msg.solution;
    bloque.is_last = msg.is_last;

    if (!msg.is_last) {
      /* Validar la solución */
      if (pow_hash(msg.solution) == msg.target) {
        bloque.is_valid = 1;

        /* Añadir la moneda al ganador*/
        sem_wait(&shm->sem_mutex);
        found_wallet = 0;

        /* Buscar la cartera */
        for (i = 0; i < shm->n_wallets; i++) {
          if (shm->wallets[i].pid == shm->winner) {
            shm->wallets[i].coins++;
            found_wallet = 1;
            break;
          }
        }

        /* Registrar la nueva cartera si no existía previamente y cabe en el array */
        if (!found_wallet && shm->n_wallets < MAX_MINERS) {
          shm->wallets[shm->n_wallets].pid = shm->winner;
          shm->wallets[shm->n_wallets].coins = 1;
          shm->n_wallets++;
        }

        sem_post(&shm->sem_mutex);

      } else {
        bloque.is_valid = 0;
      }
    }

    /* Informar a Monitor a través de Productor-Consumidor */
    sem_wait(&shm->sem_empty); /* Down(sem_empty); */
    sem_wait(&shm->sem_mutex); /* Down(sem_mutex); */
    /* AñadirElemento(); */
    shm->buffer[shm->in] = bloque; /* Insertar nuevo elemento */
    shm->in = (shm->in + 1) % 6;   /* Actualizar índice nuevo elemento en buffer circular */
    sem_post(&shm->sem_mutex);     /* Up(sem_mutex); */
    sem_post(&shm->sem_fill);      /* Up(sem_fill); */

    /* Terminar si el programa ha terminado */
    if (bloque.is_last) {
      break;
    }

    /* Esperar el lag */
    usleep(lag_comprobador * 1000);
  }

  printf("\x1b[35m[%d] Finishing (Comprobador)\x1b[0m\n", getpid());
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