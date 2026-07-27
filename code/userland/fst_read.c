/* File system test - Reader */
#include "syscall.h"
#include "lib.c"

int main() {
    myPuts("FST_Reader: Abriendo test.txt para leer...");
    OpenFileId id = Open("test.txt");
    if (id < 0) {
        myPuts("FST_Reader: Error al abrir el archivo.");
        Exit(1);
    }
    
    myPuts("FST_Reader: Leyendo...");
    char buffer[11];
    int totalRead = 0;
    while (Read(buffer, 10, id) > 0) {
        buffer[10] = '\0';
        totalRead += 10;
    }
    
    char msg[100];
    PrintString("FST_Reader: Total leido: ");
    itoa(totalRead, msg);
    myPuts(msg);
    
    myPuts("FST_Reader: Cerrando...");
    Close(id);
    myPuts("FST_Reader: Terminado.");
    Exit(0);
    return 0;
}
