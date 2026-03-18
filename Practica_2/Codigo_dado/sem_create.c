#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Constante que sirve como nombre para un semáforo con nombre */
#define SEM_NAME "/example_sem"

void sem_print(sem_t* sem) {
  int sval;
  /* Se obtiene el valor del semáforo, si es -1 se registra el error, se borra su nombre del sistema y se mata el proceso */
  if (sem_getvalue(sem, &sval) == -1) {
    perror("sem_getvalue");
    sem_unlink(SEM_NAME);
    exit(EXIT_FAILURE);
  }
  /* Si no da error se imprime su valor :3 */
  printf("Semaphore value: %d\n", sval);
  fflush(stdout);
}

int main(void) {
  /* Puntero al semáforo */
  sem_t* sem = NULL;
  pid_t pid;
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

  /*  Demostración del comportamiento antes del fork: valor 0 → post → 1 → post → 2 → wait → 1*/
  sem_print(sem); /* Imprime el valor del semáforo: 0 [Bloqueado]*/
  sem_post(sem);  /* Hace UP del semáforo */
  sem_print(sem); /* Imprime el valor del semáforo: 1 */
  sem_post(sem);  /* Hace UP del semáforo */
  sem_print(sem); /* Imprime el valor del semáforo: 2 */
  sem_wait(sem);  /* hace DOWN del semáforo */
  sem_print(sem); /* Imprime el valor del semáforo: 1 */

  /* Se hace un fork del proceso, padre e hijo comparten semáforo, el valor actual del semáforo es 1 */
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  /* EXTRA:
  - sem_trywait: DOWN no bloqueante
  - sem_timewait: DOWN bloqueante
  - sem_getvalue: duh (devuelve el valor del semáforo)*/

  if (pid == 0) {
    sem_wait(sem); /* El hijo intenta hacer DOWN, como el valor es 1, lo decrementa a 0 y entra en la sección crítica SIN bloquearse */
    /* Si padre e hijo llegaran simultáneamente, solo uno entraría y el otro se bloquearía hasta que el primero hiciera sem_post (exclusión mutua) */
    printf("Critical region (child)\n");
    sleep(5);
    printf("End of critical region (child)\n");
    sem_post(sem);      /* Si estaba ocupado, se libera el semáforo con UP */
    sem_close(sem);     /* Se cierra el semáforo liberando los recursos que el proceso tuviera asignado para ese semáforo. Sin embargo, el semáforo
                           seguirá accesible a otros procesos (no se borrará), se reduce el número de procesos asociado a él en 1 */
    exit(EXIT_SUCCESS); /* Fin del proceso hijo */
  } else {
    sem_wait(sem); /* EL padre también intenta acceder a la región crítica , si el hijo llegó primero el padre espera */
    printf("Critical region (parent)\n");
    sleep(5);
    printf("End of critical region (parent)\n");
    sem_post(sem);        /* Si estaba ocupado, se libera el semáforo con UP */
    sem_close(sem);       /* Se cierra el semáforo liberando los recursos que el proceso tuviera asignado para ese semáforo. Sin embargo, el semáforo
                                  seguirá accesible a otros procesos (no se borrará), se reduce el número de procesos asociado a él en 1 */
    sem_unlink(SEM_NAME); /* Elimina el nombre del semáforo del sistema (no tiene por qué borrarse inmediatamente), liberando los recursos asociados
                             al semáforo. Los semáforos se crean en /dev/shm [Linux] */

    wait(NULL); /* El padre espera a que el hijo termine antes de salir. */
    /* Se mata al padre */
    exit(EXIT_SUCCESS);
  }
}
