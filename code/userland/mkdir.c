/// Create new directory with given name.

#include "syscall.h"
#include "lib.c"

#define USAGE        "Usage: mkdir <directory> [<directory2> ...]"
#define MKDIR_ERROR  "Error: could not create directory."

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        PrintString(USAGE);
        Exit(1);
    }

    int error = 0;
    for (unsigned i = 1; i < argc; i++) {
        if (Mkdir(argv[i]) < 0) {
            PrintString(MKDIR_ERROR);
            error = 1;
        }
    }

    return error;
}
