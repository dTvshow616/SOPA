#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Nombres de los semáforos que se usarán */
#define SEM_NAME_A "/example_sem_1"
#define SEM_NAME_B "/example_sem_2"

/* El objetivo es forzar la secuencia: 1 → 2 → 3 → 4
usando dos semáforos para sincronizar padre e hijo.
sem1 controla cuándo puede avanzar el hijo (de 1 a 3).
sem2 controla cuándo puede avanzar el padre (de 2 a 4).
Ambos semáforos empiezan en 0 → cualquier wait bloqueará hasta recibir un post */

int main(void) {
  /* Punteros a los semáforos */
  sem_t* sem1 = NULL;
  sem_t* sem2 = NULL;
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
  if ((sem1 = sem_open(SEM_NAME_A, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  if ((sem2 = sem_open(SEM_NAME_B, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  /* Se hace un fork del proceso, padre e hijo comparten ambos semáforos, el valor actual de ambos es 0 */
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    /* Insert code A. <- HECHO */
    // NOTE - A: El hijo NO necesita esperar nada para imprimir "1" (es el primero).
    //        sem_wait(sem1) aquí bloquearía, así que no va. No hay código A necesario.
    printf("1\n");
    /* Insert code B. <- HECHO */
    // NOTE - B: El hijo avisa al padre de que puede imprimir "2".
    //        sem_post(sem2) desbloquea al padre que espera en sem_wait(sem2).
    //        Luego el hijo espera a que el padre imprima "2" antes de seguir con "3".
    //        sem_post(sem2);  → desbloquea al padre para que imprima "2"
    //        sem_wait(sem1);  → el hijo espera hasta que el padre confirme que ya imprimió "2"
    printf("3\n");
    /* Insert code C. <- HECHO */
    // NOTE - C: El hijo avisa al padre de que puede imprimir "4" y terminar.

    sem_close(sem1);
    sem_close(sem2);
  } else {
    /* Insert code D. <- HECHO */
    // NOTE - D: El padre espera a que el hijo imprima "1" primero.
    //        sem_wait(sem2) bloquea hasta que el hijo haga sem_post(sem2).
    printf("2\n");
    /* Insert code E. <- HECHO */
    // NOTE - E: El padre avisa al hijo de que ya imprimió "2", y este puede continuar con "3".
    //        Luego el padre espera a que el hijo imprima "3".
    //        sem_post(sem1); → desbloquea al hijo para que imprima "3"
    //        sem_wait(sem2); → el padre espera hasta que el hijo confirme que ya imprimió "3"
    printf("4\n");
    /* Insert code F. <- HECHO */
    // NOTE - F: No hay más sincronización necesaria; el padre limpia y espera al hijo.

    /* Se cierran los semáforos liberando los recursos que el proceso tuviera asignado para ellos. Sin embargo, los semáforos seguirán accesibles a
     * otros procesos (no se borrarán), se reduce el número de procesos asociado a ellos en 1 */
    sem_close(sem1);
    sem_close(sem2);
    /* Elimina los nombres de los semáforos del sistema (no tienen por qué borrarse inmediatamente), liberando los recursos asociados a ellos. Los
     * semáforos se crean en /dev/shm [Linux] */
    sem_unlink(SEM_NAME_A);
    sem_unlink(SEM_NAME_B);
    /* El padre espera a que el hijo termine antes de salir. */
    wait(NULL);
    /* Se mata al padre */
    exit(EXIT_SUCCESS);
  }
}
