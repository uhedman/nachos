#include "syscall.h"


int
main(void)
{
    SpaceId    newProc;
    OpenFileId input  = CONSOLE_INPUT;
    OpenFileId output = CONSOLE_OUTPUT;
    char       prompt[2] = { '-', '-' };
    char       ch, buffer[60];
    int        i;

    for (;;) {
        Write(prompt, 2, output);
        i = 0;
        do {
            Read(&buffer[i], 1, input);
        } while (buffer[i++] != '\n');

        buffer[--i] = '\0';

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
