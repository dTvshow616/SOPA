#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// NOTE - sig_atomic_t garantiza que la lectura/escritura de esta variable es atómica
//        a nivel de señales: no puede quedar a medias si el handler interrumpe al
//        hilo principal justo en medio de una asignación. volatile le dice al compilador
//        que no optimice (cachee en registro) esta variable, ya que puede cambiar
//        en cualquier momento desde el handler.
static volatile sig_atomic_t got_signal = 0;

/* Handler function for the signal SIGINT. */
// NOTE - El handler solo activa un flag. Esto es la práctica recomendada:
//        los handlers deben hacer el mínimo trabajo posible porque se ejecutan
//        de forma asíncrona e interrumpen código que puede no ser reentrante
//        (p.ej. printf no es async-signal-safe).
void handler(int sig) { got_signal = 1; }

int main(void) {
  struct sigaction act;

  act.sa_handler = handler;
  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;

  if (sigaction(SIGINT, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  // NOTE - Patrón "flag + lógica en el bucle principal": el handler solo avisa,
  //        y es el flujo normal del programa quien reacciona de forma segura.
  //        Esto evita llamar a printf u otras funciones no reentrantes dentro del handler.
  while (1) {
    printf("Waiting Ctrl+C (PID = %d)\n", getpid());
    if (got_signal) {
      got_signal = 0;  // NOTE - Se resetea antes de procesar para no perder señales posteriores.
      printf("Signal received.\n");
    }
    // NOTE - sleep() será interrumpido por SIGINT, lo que hace que el bucle
    //        comprube got_signal inmediatamente tras recibir la señal.
    sleep(9999);
  }
}
