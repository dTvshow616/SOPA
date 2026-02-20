#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Variables de control */
#ifndef ERR
#define ERR -1
#define OK (!(ERR))
#endif

/* Escribir un programa que abra un fchero indicado por el primer parámetro en modo lectura usando la función fopen. En caso de error de apertura, el
 * programa mostrará el mensaje de error correspondiente por pantalla usando perror. */
int main(int argc, char* argv[]) {
  char filename[50];
  FILE* f = NULL;

  if (argc != 2) {
    return ERR;
  }

  strcpy(filename, argv[2]);
  f = fopen(filename, "r");
  if (!f) {
    /* Se imprime el mensaje de error */
    printf("Código de error: %i\n", errno);
    perror("No se pudo abrir el fichero");
    return ERR;
  }

  fclose(f);
  return OK;
}