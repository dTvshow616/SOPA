#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_NAME "/example_sem"

/* No hace nada visible, pero su mera existencia cambia el comportamiento del SO. Sin él, SIGINT terminaría el proceso y sem_wait nunca retornaría.
Con él, sem_wait es interrumpido (retorna -1 con errno = EINTR) y el flujo continúa en main */
void handler(int sig) { return; }

int main(void) {
  /* Puntero al semáforo */
  sem_t* sem = NULL;
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

  /**
   * @brief Crea un semáforo con nombre
   *
   * @param name nombre del semáforo a crear, en este caso una constante
   * @param oflag entero que determina la operación a realizar (O_CREAT o O_EXCL), O_CREAT | O_EXCL falla si ya existe (evita colisiones)
   * @param mode_t (opcionasi se creal) permisos del semáforo, en este caso read & write
   * @param int (opcional si se crea) valor inicial del semáforo, en este caso 0 (cualquier sem_wait bloqueará hasta que alguien haga sem_post)
   *
   * @return puntero al nuevo semáforo (sem_t)
   */
  if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  sigemptyset(&(act.sa_mask)); /* Inicializa el conjunto set como vacío (sin señales) */
  /* El flag SA_RESTART NO está activo (act.sa_flags = 0), así que si SIGINT llega mientras el proceso está bloqueado en sem_wait, la llamada al
   * sistema se interrumpe y retorna con EINTR. Si SA_RESTART estuviera activo, sem_wait se reanudaría automáticamente.*/
  act.sa_flags = 0;
  /* The handler for SIGINT is set. */
  act.sa_handler = handler; /* Se registra handler_SIGALRM como la rutina de tratamiento de señal (forma POSIX recomendada) */
  if (sigaction(SIGINT, &act, NULL) < 0) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  printf("Starting wait (PID=%d)\n", getpid());
  /* El proceso se bloquea aquí esperando que el semáforo sea > 0.
  Dos formas de salir:
    1) Otro proceso hace sem_post (el valor sube a 1 y sem_wait lo consume).
    2) Llega SIGINT: el handler se ejecuta y sem_wait retorna -1 (EINTR).
  Las señales pueden "despertar" llamadas bloqueantes. */
  sem_wait(sem);
  printf("Finishing wait\n");
  sem_unlink(SEM_NAME); /* Elimina el nombre del semáforo del sistema (no tiene por qué borrarse inmediatamente), liberando los recursos asociados al
                           semáforo. Los semáforos se crean en /dev/shm [Linux] */
}
