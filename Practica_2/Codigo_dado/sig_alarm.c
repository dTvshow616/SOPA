#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SECS 10

/* The handler shows a message and ends the process. */
// NOTE - El handler se ejecuta de forma asíncrona cuando el proceso recibe SIGALRM.
//        El SO interrumpe el flujo normal (aunque esté en medio del bucle for)
//        y salta aquí. Al llamar exit(), el proceso termina limpiamente.
void handler_SIGALRM(int sig) {
  printf("\nThese are the numbers I had time to count\n");
  exit(EXIT_SUCCESS);
}

int main(void) {
  struct sigaction act;
  long int i;

  // NOTE - sigemptyset vacía la máscara de señales del handler: durante su ejecución
  //        no se bloqueará ninguna señal adicional.
  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;

  /* The handler for SIGALRM is set. */
  // NOTE - Se registra handler_SIGALRM como manejador de SIGALRM mediante sigaction,
  //        que es la forma POSIX recomendada (más robusta que la antigua signal()).
  act.sa_handler = handler_SIGALRM;
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // NOTE - alarm(SECS) le pide al kernel que envíe SIGALRM a este proceso
  //        transcurridos SECS segundos. Solo puede haber un alarm activo por proceso;
  //        si ya había uno, devuelve los segundos restantes (de ahí el aviso).
  if (alarm(SECS)) {
    fprintf(stderr, "There is a previously established alarm\n");
  }

  // NOTE - Bucle infinito que cuenta lo más rápido posible.
  //        Pasados SECS segundos, el kernel interrumpe este bucle con SIGALRM,
  //        salta al handler y el proceso termina. El código tras el for es inalcanzable.
  fprintf(stdout, "The count starts (PID=%d)\n", getpid());
  for (i = 0;; i++) {
    fprintf(stdout, "%10ld\r", i);
    fflush(stdout);
  }

  fprintf(stdout, "End of program\n");
  exit(EXIT_SUCCESS);
}
