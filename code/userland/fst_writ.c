/* File system test - Writer */
#include "syscall.h"
#include "lib.c"

int main() {
    myPuts("FST_Writer: Abriendo test.txt para escribir...");
    OpenFileId id = Open("test.txt");
    if (id < 0) {
        myPuts("FST_Writer: Error al abrir el archivo.");
        Exit(1);
    }
    
    myPuts("FST_Writer: Escribiendo...");
    for (int i = 0; i < 50; i++) {
        Write("1234567890", 10, id);
    }
    
    myPuts("FST_Writer: Cerrando...");
    Close(id);
    myPuts("FST_Writer: Terminado.");
    Exit(0);
    return 0;
}
