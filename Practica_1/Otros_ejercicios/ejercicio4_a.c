#include <stdio.h>
#include <time.h>

/* Variables de control */
#ifndef ERR
#define ERR -1
#define OK (!(ERR))
#endif

/* Escribir un programa que realice una espera de 10 segundos usando la función clock en un bucle. Ejecutar en otra terminal el comando top. ¿Qué se
 * observa? */
int main(int argc, char* argv[]) {
  clock_t start, end = 0;
  int secs_passed = 0;

  start = clock();

  while (secs_passed != 10) {
    end = clock();

    /* Si ha pasado un segundo, imprímelo :3 */
    if (((end - start) - secs_passed * CLOCKS_PER_SEC) == 1 * CLOCKS_PER_SEC) {
      secs_passed++;
      if (secs_passed == 10) {
        break;
      }

      printf("%i...\n", secs_passed);
    }
  }

  printf("10!\n");

  return OK;
}