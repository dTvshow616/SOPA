#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_NAME "/example_sem"

void sem_print(sem_t* sem) {
  int sval;
  // NOTE - sem_getvalue lee el valor actual del semáforo (número de recursos disponibles).
  //        Un valor > 0 significa que hay "tokens" disponibles; 0 significa que
  //        el siguiente sem_wait bloqueará al proceso.
  if (sem_getvalue(sem, &sval) == -1) {
    perror("sem_getvalue");
    sem_unlink(SEM_NAME);
    exit(EXIT_FAILURE);
  }
  printf("Semaphore value: %d\n", sval);
  fflush(stdout);
}

int main(void) {
  sem_t* sem = NULL;
  pid_t pid;

  // NOTE - sem_open crea un semáforo con nombre en el sistema de archivos POSIX (/dev/shm
  //        en Linux). O_CREAT | O_EXCL falla si ya existe (evita colisiones).
  //        El valor inicial es 0: cualquier sem_wait bloqueará hasta que alguien haga sem_post.
  if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  // NOTE - Demostración del comportamiento antes del fork:
  //        valor 0 → post → 1 → post → 2 → wait → 1
  sem_print(sem);  // valor: 0
  sem_post(sem);   // valor: 1  (señal: "hay un recurso disponible")
  sem_print(sem);  // valor: 1
  sem_post(sem);   // valor: 2
  sem_print(sem);  // valor: 2
  sem_wait(sem);   // valor: 1  (consume un recurso; no bloquea porque valor > 0)
  sem_print(sem);  // valor: 1

  // NOTE - fork() duplica el proceso. El hijo hereda el descriptor del semáforo,
  //        así que ambos comparten el MISMO semáforo con nombre.
  //        El valor actual (1) es visible para los dos.
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    // NOTE - El hijo hace sem_wait: como el valor es 1, lo decrementa a 0
    //        y entra en la sección crítica SIN bloquearse.
    //        Si padre e hijo llegaran simultáneamente, solo uno entraría;
    //        el otro bloquearía hasta que el primero hiciera sem_post.
    //        Esto es exclusión mutua con semáforos.
    sem_wait(sem);
    printf("Critical region (child)\n");
    sleep(5);
    printf("End of critical region (child)\n");
    sem_post(sem);  // NOTE - Libera el semáforo: el otro proceso puede entrar ahora.
    sem_close(sem);
    exit(EXIT_SUCCESS);
  } else {
    // NOTE - El padre también compite por la sección crítica.
    //        Según la planificación del SO, puede que bloquee aquí
    //        si el hijo ya tomó el semáforo primero.
    sem_wait(sem);
    printf("Critical region (parent)\n");
    sleep(5);
    printf("End of critical region (parent)\n");
    sem_post(sem);
    sem_close(sem);
    // NOTE - sem_unlink elimina el nombre del semáforo del sistema de archivos.
    //        El semáforo sigue existiendo en memoria hasta que todos lo cierren,
    //        pero ya no es accesible por nombre. Solo el padre hace unlink.
    sem_unlink(SEM_NAME);

    wait(NULL);  // NOTE - El padre espera a que el hijo termine antes de salir.
    exit(EXIT_SUCCESS);
  }
}
