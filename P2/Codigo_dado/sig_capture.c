#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/* Handler function for the signal SIGINT. */
// NOTE - Handler para SIGINT (Ctrl+C). Sin este registro, la acción por defecto
//        de SIGINT sería terminar el proceso. Al capturarla, el proceso sobrevive
//        y simplemente imprime un mensaje cada vez que se pulsa Ctrl+C.
void handler(int sig) {
  printf("Signal number %d received\n", sig);
  fflush(stdout);
}

int main(void) {
  struct sigaction act;

  act.sa_handler = handler;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;

  // NOTE - sigaction asocia el handler a SIGINT en este proceso.
  //        A partir de aquí, Ctrl+C no mata el proceso sino que llama a handler().
  if (sigaction(SIGINT, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // NOTE - El proceso queda en un bucle durmiendo. sleep() puede ser interrumpido
  //        antes de tiempo por una señal: cuando llega SIGINT, sleep() retorna,
  //        el handler se ejecuta y el bucle vuelve a empezar.
  while (1) {
    printf("Waiting Ctrl+C (PID = %d)\n", getpid());
    sleep(9999);
  }
}
