#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
  int sig;
  pid_t pid;

  // NOTE - El programa espera exactamente dos argumentos: -<señal> y <pid>.
  //        Ejemplo de uso: ./sig_kill -9 1234  (equivalente a kill -9 1234)
  if (argc != 3) {
    fprintf(stderr, "Usage: %s -<signal> <pid>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // NOTE - argv[1] tiene la forma "-N" (p.ej. "-9"). Se salta el '-' con +1
  //        y se convierte el número de señal a entero.
  sig = atoi(argv[1] + 1);
  // NOTE - argv[2] es el PID destino. Se hace cast explícito a pid_t
  //        para que el tipo sea el correcto en la llamada a kill().
  pid = (pid_t)atoi(argv[2]);

  /* Complete the code. */

  exit(EXIT_SUCCESS);
}
