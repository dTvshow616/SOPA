#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
  /**
   * @brief Es una máscara de señales que indica las señales bloquedas.
   *
   * @param set la máscara a aplicar
   * @param oset la máscara anterior (por si se quisiera restaurar el estado original)
   */
  sigset_t set, oset;

  /* Mask to block signals SIGUSR1 and SIGUSR2. */
  sigemptyset(&set);        /* Inicializa el conjunto set como vacío (sin señales) */
  sigaddset(&set, SIGUSR1); /* Añade la señal SIGSUR1 al conjunto set */
  sigaddset(&set, SIGUSR2); /* Añade la señal SIGSUR2 al conjunto set */

  /* Blocking of signals SIGUSR1 and SIGUSR2 in the process. */
  /**
   * @brief Una vez definida la máscara de señales, se puede aplicar al proceso mediante sigprocmask(), hasta que no se invoque no se cambia la
   * máscara del proceso
   *
   * @param how int que indica cómo se modificará la máscara del proceso, 3 opciones:
   *  1. SIG_BLOCK: la máscara del proceso resultante sea el resultado de la unión de la máscara actual el conjunto pasado como argumento (masc_proc =
   * set + oset)
   *  2. SIG_SETMASK: la máscara del proceso resultante sea la indicada en el conjunto (masc_proc = set)
   *  3. SIG_UNBLOCK: las señales incluidas en el conjunto quedarán desbloqueadas en la nueva máscara de señales (masc_proc = set - oset)
   *
   * @param set puntero al conjunto que se utilizará para modi car la máscara de señales
   * @param oldset  puntero al conjunto donde se almacenará la máscara de señales anterior, de manera que se pueda restaurar con posterioridad
   *
   * (Una señal bloqueada no se descarta, queda pendiente hasta que se desbloquee)
   * Si el proceso es multihilo se debe usar pthread_sigmask() en vez de esta
   * Se pueden ver las señales bloqueadas (pendientes) con sigpending()
   */
  if (sigprocmask(SIG_BLOCK, &set, &oset) < 0) { /* mas_proc = set + oset */
    perror("sigprocmask");
    exit(EXIT_FAILURE);
  }

  printf("Waiting signals (PID = %d)\n", getpid());
  printf("SIGUSR1 and SIGUSR2 are blocked\n");
  /* pause() produce una spera no activa,  deja al proceso en espera hasta que reciba una señal no bloqueada (no SIGSUR1 ni SIGSUR2). Si llega
   * SIGUSR1/SIGUSR2 mientras está bloqueada, quedará pendiente y se entregará cuando se desbloquee*/
  pause();

  printf("End of program\n");
  exit(EXIT_SUCCESS);
}
