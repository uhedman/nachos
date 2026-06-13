#include "syscall.h"
#include "lib.c"


int
main(void)
{
    SpaceId newProc;
    const char prompt[] = "--";
    char       buffer[60];
    int        i;

    for (;;) {
        PrintString(prompt);
        i = ReadLine(buffer, sizeof(buffer));

        if (i > 0) {
            int background = 0;
            char *cmd = buffer;
            if (buffer[0] == '&') {
                background = 1;
                cmd = &buffer[1];
                while (*cmd == ' ' || *cmd == '\t') {
                    cmd++;
                }
            }
            if (*cmd != '\0') {
                newProc = Exec(cmd, !background);
                if (newProc != -1 && !background) {
                    Join(newProc);
                }
            }
        }
    }

    return -1;
}
