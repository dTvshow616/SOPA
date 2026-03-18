#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @brief Flag global activada por handler para saber si se ha recibido alguna señal, se usa sig_atomic_t porque garantiza que la lectura/escritura de
 * esta variable es atómica aka no puede quedar a medias si el handler interrumpe al hilo principal justo en medio de una asignación.
 *
 * "volatile" sirve para que el compilador no optimice esta variable ya que puede cambiar en cualquier momento desde el handler.
 *
 * Lo suyo es que el progama sea el que reaccione de forma segura al flag
 *
 */
static volatile sig_atomic_t got_signal = 0;

/* Handler function for the signal SIGINT. */
/* Es recomendable que el manejador simplemente controle que se ha recibido la señal (en este caso activa una flag global) y que el código relacionado
 * con la misma se ejecute dentro del flujo normal del programa */
void handler(int sig) { got_signal = 1; }

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
   * @param sa_flags [int sa_flags] banderas para modidicar el comportamiento
   */
  struct sigaction act;

  act.sa_handler = handler;
  sigemptyset(&(act.sa_mask)); /* Clear all signals from SET */
  act.sa_flags = 0;

  /**
   * @brief es una función de las librerías uqe permite modificar el comportamiento de un proceso al recibir una señal
   *
   * @param signum número de la señal a capturar
   * @param act puntero a una estructura sigaction
   * @param oldact puntero a una estructura de tipo sigaction donde se almacenará la acción establecida previamente, para poder recuperarla más
   * adelante
   */
  if (sigaction(SIGINT, &act, NULL) < 0) { /* Sigaction captura SIGINT (Ctrl + C) y llama a handler() a partir de act.sa_handler*/
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  while (1) {
    printf("Waiting Ctrl+C (PID = %d)\n", getpid());
    if (got_signal) { /* Si se había recibido una señal en el anterior bucle se reseta para no perder señales posteriores */
      got_signal = 0;
      printf("Signal received.\n");
    }
    sleep(9999); /* 1. Pulsas Ctrl + C mientras sleep() está activo */
    /* 2. Se interrumpe sleep y se ejecuta handler(), que activa el flag got_signal*/
    /* 3. handler() termina y se ejecuta otra vez el bucle */
  }
}
