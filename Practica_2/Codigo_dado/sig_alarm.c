#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SECS 10

/* The handler shows a message and ends the process. */
void handler_SIGALRM(int sig) {
  /* Se ejecuta al saltar la alarma y termina el proceso */
  printf("\nThese are the numbers I had time to count\n");
  exit(EXIT_SUCCESS);
}

int main(void) {
  /**
   * @brief Es una estructura que define el comportamiento cuando se recibe una señal
   *
   * @param sa_handler función [void (*sa_handler)(int)] que define la acción que se tomará al recibir la señal, tres posibles entradas:
   *  1. SIG_DFL: acción por defecto asociada a la señal (manejador por defecto). Por lo general, esta acción consiste en terminar el proceso, y en
   * algunos casos también incluye generar un chero core.
   *  2. SIG_IGN: ignora la señal
   *  3. <dirección de la rutina de tratamiento de señal> (manejador suministrado por el usuario)
   * @param sa_sigaction [void (*sa_sigaction)(int, siginfo_t *, void *)] la acción que se tomará al recibir la señal pero definida con este prototipo
   * extendido, que se usa cuando se incluye SA_SIGINFO como bandera, permite añadir información adicional
   * @param sa_mask [sigset_t sa_mask]  máscara de señales adicionales que se bloquearán durante la ejecución del manejador (la señal que se captura
   * se bloquea por defecto, salvo que se indique lo contrario).
   * @param sa_flags [int sa_flags] banderas para modificar el comportamiento
   */
  struct sigaction act;
  long int i;

  sigemptyset(&(act.sa_mask)); /* Inicializa el conjunto set como vacío (sin señales) */
  act.sa_flags = 0;

  /* The handler for SIGALRM is set. */
  act.sa_handler = handler_SIGALRM; /* Se registra handler_SIGALRM como la rutina de tratamiento de señal (forma POSIX recomendada) */
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  /* Se crea la alarm, esta envía SIGALRM al proceso pasados SECS segundos.
  Solo puede haber un alarm activo por proceso, si ya había uno, devuelve los segundos restantes */
  if (alarm(SECS)) {
    fprintf(stderr, "There is a previously established alarm\n");
  }

  fprintf(stdout, "The count starts (PID=%d)\n", getpid());
  for (i = 0;; i++) { /* Bucle infinito hasta que salte la alarma y se active el handler */
    fprintf(stdout, "%10ld\r", i);
    fflush(stdout);
  }

  fprintf(stdout, "End of program\n"); /* Esto nunca llega a imprimise */
  exit(EXIT_SUCCESS);
}
