#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
  // NOTE - sigset_t es una máscara de bits donde cada bit representa una señal.
  //        'set' será la máscara a aplicar; 'oset' guardará la máscara anterior
  //        (útil si luego se quiere restaurar el estado original).
  sigset_t set, oset;

  /* Mask to block signals SIGUSR1 and SIGUSR2. */
  // NOTE - Se parte de una máscara vacía y se añaden SIGUSR1 y SIGUSR2.
  //        Estas son señales de usuario sin semántica fija: el programador
  //        decide qué significan en su aplicación.
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGUSR2);

  /* Blocking of signals SIGUSR1 and SIGUSR2 in the process. */
  // NOTE - sigprocmask(SIG_BLOCK, ...) añade las señales de 'set' a la máscara
  //        de bloqueo del proceso. Una señal bloqueada NO se descarta: queda
  //        pendiente en el kernel hasta que se desbloquee. SIG_BLOCK acumula;
  //        SIG_SETMASK reemplazaría la máscara entera; SIG_UNBLOCK desbloquearía.
  if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) {
    perror("sigprocmask");
    exit(EXIT_FAILURE);
  }

  printf("Waiting signals (PID = %d)\n", getpid());
  printf("SIGUSR1 and SIGUSR2 are blocked\n");
  // NOTE - pause() suspende el proceso hasta que llegue CUALQUIER señal no bloqueada.
  //        Como SIGUSR1 y SIGUSR2 están bloqueadas, no despertarán al proceso.
  //        Solo una señal distinta (p.ej. SIGTERM, SIGINT) lo hará continuar.
  //        Si llega SIGUSR1/SIGUSR2 mientras está bloqueada, quedará pendiente
  //        y se entregará cuando se desbloquee.
  pause();

  printf("End of program\n");
  exit(EXIT_SUCCESS);
}
