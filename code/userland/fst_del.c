/* File system test - Deleter */
#include "syscall.h"
#include "lib.c"

int main() {
    myPuts("FST_Deleter: Eliminando test.txt...");
    int ret = Remove("test.txt");
    if (ret == 0) {
        myPuts("FST_Deleter: Archivo eliminado correctamente (o marcado para eliminar).");
    } else {
        myPuts("FST_Deleter: Error al eliminar (no encontrado).");
    }
    Exit(0);
    return 0;
}
