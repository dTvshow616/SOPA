#ifndef _MINER_RUSH_H
#define _MINER_RUSH_H

/* La descripcion de las librerias a continuacion se ha sacado del man siempre que se podia */
#include <errno.h>     /* Errno, EINTR */
#include <fcntl.h>     /* Manipulate file descriptor */
#include <mqueue.h>    /* POSIX API for message queues */
#include <pthread.h>   /* POSIX threads */
#include <semaphore.h> /* Semaphores to manage the different processes*/
#include <signal.h>    /* Signal handling */
#include <stdint.h>    /* Exact-width integer types */
#include <stdio.h>     /* Standard input/output library functions */
#include <stdlib.h>    /* Numeric conversion functions, pseudo-random numbers generation functions, memory allocation, process control functions */
#include <sys/mman.h>  /* File mapping */
#include <sys/stat.h>  /* Display of file or file system's status */
#include <sys/types.h> /* Defines a collection of typedef symbols and structures */
#include <sys/wait.h>  /* Wait for process to change state */
#include <time.h>      /* Defines four variable types, two macro and various functions for manipulating date and time */
#include <unistd.h>    /* POSIX operating system API */

#define SHM_NAME "/shared_memory" /* Memoria compartida de todos los procesos */
#define SEM_NAME "/semaphore"     /* Semáforo para gestionar el acceso al fichero de mineros */
#define MQ_NAME "/message_queue"  /* Cola de mensajes entre procesos */

#define MAX_MINERS 100   /* Número máximo de mineros que puede haber */
#define MAX_VOTE_WAIT 50 /* Tiempo máximo a esperar por los votos de los mineros (en segundos) */

/**
 * @brief Estructura de la memoria compartida, toda la info de los ficheros ahora va aquí
 */
typedef struct {
  /* Datos del antiguo miners.txt */
  pid_t miners[MAX_MINERS]; /* Lista de mineros activos (sus pids) */
  int n_miners;             /* Número de mineros actual */

  /* Datos del antiguo target.txt */
  int target; /* Número a buscar */

  /* Datos del antiguo winner.txt */
  pid_t winner;        /* PID del ganador */
  int winner_solution; /* Solución encontrada por el ganador */

  /* Datos del antiguo voters.txt */
  pid_t voter[MAX_MINERS];     /* PIDs de los votantes */
  char voter_vote[MAX_MINERS]; /* Votos de los votantes */
  int n_votes;                 /* Número de votantes */

  /* Datos del antiguo participants.txt */
  pid_t participants[MAX_MINERS]; /* Lista de participantes activos (sus pids) */
  int n_participants;             /* Número de participantes actual */

  /* Carteras de los mineros */
  int wallets[MAX_MINERS];

} Shared_Memory;

/**
 * @brief Mensaje que viaja de Minero a Comprobador por la cola de mensajes
 */
typedef struct {
  long target;   /* El target a buscar por el minero */
  long solution; /* La solución encontrada por el minero */
  int is_last;   /* 1 si el minero era el último, 0 si no */
} Minero_Comprobador;

/**
 * @brief Atributos de la cola de mensajes entre Minero y Comprobador
 */
struct mq_attr attributes = {.mq_flags = 0, .mq_maxmsg = 7, .mq_curmsgs = 0, .mq_msgsize = sizeof(Minero_Comprobador)};

#endif