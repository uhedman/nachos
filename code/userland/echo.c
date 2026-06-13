/// Outputs arguments entered on the command line.

#include "syscall.h"
#include "lib.c"

int
main(int argc, char *argv[])
{
    for (unsigned i = 1; i < argc; i++) {
        if (i != 1) {
            PrintChar(' ');
        }
        PrintString(argv[i]);
    }
    PrintChar('\n');
}
