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

#define MAX_VOTE_WAIT 50 /* Tiempo máximo a esperar por los votos de los mineros (en segundos) */

#endif