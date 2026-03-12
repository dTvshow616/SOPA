#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SEM_NAME_A "/example_sem_1"
#define SEM_NAME_B "/example_sem_2"

// NOTE - El objetivo es forzar la secuencia: 1 → 2 → 3 → 4
//        usando dos semáforos para sincronizar padre e hijo.
//        sem1 controla cuándo puede avanzar el hijo (de 1 a 3).
//        sem2 controla cuándo puede avanzar el padre (de 2 a 4).
//        Ambos semáforos empiezan en 0 → cualquier wait bloqueará hasta recibir un post.

int main(void) {
  sem_t* sem1 = NULL;
  sem_t* sem2 = NULL;
  pid_t pid;

  // NOTE - Se crean dos semáforos con nombre, valor inicial 0.
  //        O_EXCL garantiza que no existían previamente (si sí, falla y hay que limpiarlos).
  if ((sem1 = sem_open(SEM_NAME_A, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  if ((sem2 = sem_open(SEM_NAME_B, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED) {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  // NOTE - Tras fork(), hijo y padre comparten los mismos semáforos con nombre.
  //       El orden de ejecución entre ellos no está garantizado por el SO,
  //       de ahí que necesitemos semáforos para imponer el orden deseado.
  pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    /* Insert code A. */
    // NOTE - A: El hijo NO necesita esperar nada para imprimir "1" (es el primero).
    //        sem_wait(sem1) aquí bloquearía, así que no va. No hay código A necesario.
    printf("1\n");
    /* Insert code B. */
    // NOTE - B: El hijo avisa al padre de que puede imprimir "2".
    //        sem_post(sem2) desbloquea al padre que espera en sem_wait(sem2).
    //        Luego el hijo espera a que el padre imprima "2" antes de seguir con "3".
    //        sem_post(sem2);  → desbloquea al padre para que imprima "2"
    //        sem_wait(sem1);  → el hijo espera hasta que el padre confirme que ya imprimió "2"
    printf("3\n");
    /* Insert code C. */
    // NOTE - C: El hijo avisa al padre de que puede imprimir "4" y terminar.

    sem_close(sem1);
    sem_close(sem2);
  } else {
    /* Insert code D. */
    // NOTE - D: El padre espera a que el hijo imprima "1" primero.
    //        sem_wait(sem2) bloquea hasta que el hijo haga sem_post(sem2).
    printf("2\n");
    /* Insert code E. */
    // NOTE - E: El padre avisa al hijo de que ya imprimió "2", y este puede continuar con "3".
    //        Luego el padre espera a que el hijo imprima "3".
    //        sem_post(sem1); → desbloquea al hijo para que imprima "3"
    //        sem_wait(sem2); → el padre espera hasta que el hijo confirme que ya imprimió "3"
    printf("4\n");
    /* Insert code F. */
    // NOTE - F: No hay más sincronización necesaria; el padre limpia y espera al hijo.

    sem_close(sem1);
    sem_close(sem2);
    sem_unlink(SEM_NAME_A);
    sem_unlink(SEM_NAME_B);
    wait(NULL);
    exit(EXIT_SUCCESS);
  }
}
