#include "lib.c"

#define USAGE "Usage: rm <filename1> [<filename2> ...]"
#define REMOVE_ERROR "Error: could not remove file "

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        myPuts(USAGE);
        return -1;
    }

    int success = 1;
    for (unsigned i = 1; i < argc; i++) {
        if (Remove(argv[i]) < 0) {
            PrintString(REMOVE_ERROR);
            myPuts(argv[i]);
            success = 0;
        }
    }

    return success;
}