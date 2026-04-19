#include <errno.h>      /* errno, EINTR */
#include <fcntl.h>      /* manipulate file descriptor */
#include <pthread.h>    /* POSIX threads */
#include <semaphore.h>  /* Semaphores to manage the different processes*/
#include <signal.h>     /* Signal handling */
#include <stdint.h>     /* exact-width integer types */
#include <stdio.h>      /* standard input/output library functions */
#include <stdlib.h>     /* numeric conversion functions, pseudo-random numbers generation functions, memory allocation, process control functions */
#include <sys/mman.h>   // TODO documentar
#include <sys/stat.h>   // TODO documentar
#include <sys/types.h>  // TODO documentar
#include <sys/wait.h>   /* wait for process to change state */
#include <time.h>       /* Tiempitos */
#include <unistd.h>     /* POSIX operating system API */

#define SHM_NAME "caca"

/* This is only a code fragment, not a complete program... */
int main() {
  int fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
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
    printf("Shared memory segment created\n");
  }
}
