/* File system test - Master */
#include "syscall.h"
#include "lib.c"

int main() {
    myPuts("--- INICIANDO TEST CONCURRENTE DEL FILE SYSTEM ---");
    
    // 1. Creador (síncrono)
    myPuts("Master: Lanzando Creador...");
    SpaceId creatorId = Exec("fst_creat", 1);
    Join(creatorId);
    
    // 2. Escritor, Lector y Borrador (concurrentes)
    // El borrador intentará borrar mientras se lee/escribe. 
    // Los bloques no deben liberarse hasta que el lector y escritor terminen.
    myPuts("Master: Lanzando Writer, Reader y Deleter concurrentemente...");
    SpaceId writerId = Exec("fst_writ", 1);
    SpaceId readerId = Exec("fst_read", 1);
    SpaceId deleterId = Exec("fst_del", 1);
    
    // Esperamos a todos
    Join(writerId);
    Join(readerId);
    Join(deleterId);
    
    myPuts("--- TEST FINALIZADO ---");
    Exit(0);
    return 0;
}
