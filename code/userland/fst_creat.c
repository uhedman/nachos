/* File system test - Creator */
#include "syscall.h"
#include "lib.c"

int main() {
    myPuts("FST_Creator: Creando test.txt...");
    Create("test.txt");
    myPuts("FST_Creator: Archivo creado.");
    Exit(0);
    return 0;
}
