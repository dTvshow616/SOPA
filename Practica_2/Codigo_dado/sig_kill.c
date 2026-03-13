#include <signal.h> /* kill() está incluida aquí */
#include <stdio.h>
#include <stdlib.h>

/* Completado en el ejercicio 2 */

int main(int argc, char* argv[]) {
  int sig;
  pid_t pid;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s -<signal> <pid>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  sig = atoi(argv[1] + 1);    /* Parseamos <signal> (el +1 es para que omita el -) */
  pid = (pid_t)atoi(argv[2]); /* Parseamos el pid */

  /* CdE */
  if (sig < 1 || sig > 31) {
    perror("The signal value is out of bounds [1-31] ");
  }

  /* Código completado: */
  if (kill(pid, sig) == -1) {
    perror("Kill failed :( ");
  }

  exit(EXIT_SUCCESS);
}
