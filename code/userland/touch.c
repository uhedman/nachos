/// Creates files specified on the command line.

#include "syscall.h"
#include "lib.c"

#define ARGC_ERROR    "Error: missing argument."
#define CREATE_ERROR  "Error: could not create file."

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        PrintString(ARGC_ERROR);
        Exit(1);
    }

    int success = 1;
    for (unsigned i = 1; i < argc; i++) {
        if (Create(argv[i]) < 0) {
            PrintString(CREATE_ERROR);
            success = 0;
        }
    }
    return !success;
}
