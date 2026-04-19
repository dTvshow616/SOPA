#ifndef _MINER_RUSH_H
#define _MINER_RUSH_H

/* La descripcion de las librerias a continuacion se ha sacado del man siempre que se podia */
#include <errno.h>      /* Errno, EINTR */
#include <fcntl.h>      /* Manipulate file descriptor */
#include <pthread.h>    /* POSIX threads */
#include <semaphore.h>  /* Semaphores to manage the different processes*/
#include <signal.h>     /* Signal handling */
#include <stdint.h>     /* Exact-width integer types */
#include <stdio.h>      /* Standard input/output library functions */
#include <stdlib.h>     /* Numeric conversion functions, pseudo-random numbers generation functions, memory allocation, process control functions */
#include <sys/mman.h>   /* File mapping */
#include <sys/stat.h>   /* Display of file or file system's status */
#include <sys/types.h>  // TODO documentar
#include <sys/wait.h>   /* Wait for process to change state */
#include <time.h>       /* Tiempitos */
#include <unistd.h>     /* POSIX operating system API */

#define SHM_NAME "/shared_memory" /* Memoria compartida de todos los procesos */
#define SEM_NAME "/semaphore"     /* Semáforo para gestionar el acceso al fichero de mineros */
#define MQ_NAME "/message_queue"  /* Cola de mensajes entre procesos */

#define MAX_MINERS 100   /* Número máximo de mineros que puede haber */
#define MAX_VOTE_WAIT 50 /* Tiempo máximo a esperar por los votos de los mineros (en segundos) */

/**
 * @brief Mensaje que viaja de Minero a Comprobador por la cola de mensajes
 */
typedef struct {
  long target;   /* El target a buscar por el minero */
  long solution; /* La solución encontrada por el minero */
  int is_last;   /* 1 si el minero era el último, 0 si no */
} Minero_Comprobador;

/**
 * @brief Estructura de la memoria compartida, toda la info de los ficheros ahora va aquí
 */
typedef struct {
  /* Datos del antiguo miners.txt */
  sem_t* miners_semaphore;  /* Semáforo para los mineros */
  pid_t miners[MAX_MINERS]; /* Lista de mineros activos (sus pids) */
  int n_miners;             /* Número de mineros actual */

  /* Aquí van el resto de cosas, voy una por una */
} Shared_Memory;

#endif