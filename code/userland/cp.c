#include "lib.c"

#define BUFFER_SIZE 256
#define USAGE "Usage: cp <source> <destination>"
#define SOURCE_ERROR "Error: could not open source file."
#define DEST_ERROR "Error: could not create destination file."
#define DEST_OPEN_ERROR "Error: could not open destination file."
#define READ_ERROR "Error: could not read from source file."
#define WRITE_ERROR "Error: could not write to destination file."

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        myPuts(USAGE);
        return -1;
    }

    char *source = argv[1];
    char *dest = argv[2];

    int fdSource = Open(source);
    if (fdSource < 0) {
        myPuts(SOURCE_ERROR);
        return -1;
    }

    if (Create(dest) < 0) {
        Close(fdSource);
        myPuts(DEST_ERROR);
        return -1;
    }

    int fdDest = Open(dest);
    if (fdDest < 0) {
        Close(fdSource);
        myPuts(DEST_OPEN_ERROR);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    int bytesRead;

    for (;;) {
        bytesRead = Read(buffer, BUFFER_SIZE, fdSource);
        if (bytesRead <= 0) {
            break;
        }

        if (Write(buffer, bytesRead, fdDest) < 0) {
            Close(fdSource);
            Close(fdDest);
            myPuts(WRITE_ERROR);
            return -1;
        }
    }

    if (bytesRead < 0) {
        Close(fdSource);
        Close(fdDest);
        myPuts(READ_ERROR);
        return -1;
    }

    Close(fdSource);
    Close(fdDest);

    return 0;
}