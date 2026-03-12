#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_NAME "/example_sem"

// NOTE - Handler mínimo para SIGINT: no hace nada visible, pero su mera existencia
//        cambia el comportamiento del SO. Sin él, SIGINT terminaría el proceso
//        y sem_wait nunca retornaría. Con él, sem_wait es interrumpido (retorna -1
//        con errno = EINTR) y el flujo continúa en main.
void handler(int sig) { return; }

int main(void) {
  sem_t* sem = NULL;
  struct sigaction act;

  // NOTE - Semáforo inicializado a 0: sem_wait bloqueará indefinidamente
  //        a menos que alguien haga sem_post o llegue una señal.
  if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  sigemptyset(&(act.sa_mask));
  act.sa_flags = 0;

  /* The handler for SIGINT is set. */
  // NOTE - Se registra el handler para SIGINT (Ctrl+C).
  //       El flag SA_RESTART NO está activo (act.sa_flags = 0), así que
  //       si SIGINT llega mientras el proceso está bloqueado en sem_wait,
  //       la llamada al sistema se interrumpe y retorna con EINTR.
  //       Si SA_RESTART estuviera activo, sem_wait se reanudaría automáticamente.
  act.sa_handler = handler;
  if (sigaction(SIGINT, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  printf("Starting wait (PID=%d)\n", getpid());
  // NOTE - El proceso se bloquea aquí esperando que el semáforo sea > 0.
  //        Dos formas de salir:
  //          1) Otro proceso hace sem_post (el valor sube a 1 y sem_wait lo consume).
  //          2) Llega SIGINT: el handler se ejecuta y sem_wait retorna -1 (EINTR).
  //        Este patrón ilustra cómo las señales pueden "despertar" llamadas bloqueantes.
  sem_wait(sem);
  printf("Finishing wait\n");
  sem_unlink(SEM_NAME);
}
