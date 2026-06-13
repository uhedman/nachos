#include "lib.c"

#define BUFFER_SIZE 256
#define USAGE "Usage: cat [<filename1> <filename2> ...]"
#define FILE_ERROR "Error: could not open file: "
#define READ_ERROR "Error: could not read from file."

int
main(int argc, char *argv[])
{
    char buffer[BUFFER_SIZE];

    if (argc == 1) {
        char ch;
        int bytesRead;

        for (;;) {
            bytesRead = Read(&ch, 1, CONSOLE_INPUT);
            if (bytesRead <= 0) {
                break;
            }

            if (Write(&ch, 1, CONSOLE_OUTPUT) < 0) {
                myPuts(READ_ERROR);
                return -1;
            }
        }
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        char *filename = argv[i];
        int fd = Open(filename);
        if (fd < 0) {
            PrintString(FILE_ERROR);
            myPuts(filename);
            return -1;
        }

        int bytesRead;
        for (;;) {
            bytesRead = Read(buffer, BUFFER_SIZE, fd);
            if (bytesRead <= 0) {
                break;
            }

            if (Write(buffer, bytesRead, CONSOLE_OUTPUT) < 0) {
                Close(fd);
                myPuts(READ_ERROR);
                return -1;
            }
        }

        if (bytesRead < 0) {
            Close(fd);
            myPuts(READ_ERROR);
            return -1;
        }

        Close(fd);
    }

    return 0;
}
