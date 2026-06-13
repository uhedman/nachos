#include "syscall.h"
#include "stdbool.h"
#include "lib.c"


#define MAX_LINE_SIZE  60
#define MAX_ARG_COUNT  32
#define ARG_SEPARATOR  ' '

static inline void
WritePrompt()
{
    static const char PROMPT[] = "--> ";
    PrintString(PROMPT);
}

static inline void
WriteError(const char *description)
{
    if (description == NULL) {
        description = "";
    }

    static const char PREFIX[] = "Error: ";
    static const char SUFFIX[] = "\n";

    PrintString(PREFIX);
    PrintString(description);
    PrintString(SUFFIX);
}


static bool
PrepareArguments(char *line, char **argv, unsigned argvSize)
{
    if (line == NULL || argv == NULL || argvSize == 0) {
        return false;
    }

    unsigned argCount;

    argv[0] = line;
    argCount = 1;

    // Traverse the whole line and replace spaces between arguments by null
    // characters, so as to be able to treat each argument as a standalone
    // string.
    //
    // TODO: what happens if there are two consecutive spaces?, and what
    //       about spaces at the beginning of the line?, and at the end?
    //
    // TODO: what if the user wants to include a space as part of an
    //       argument?
    for (unsigned i = 0; line[i] != '\0'; i++) {
        if (line[i] == ARG_SEPARATOR) {
            if (argCount == argvSize - 1) {
                // The maximum of allowed arguments is exceeded, and
                // therefore the size of `argv` is too.  Note that 1 is
                // decreased in order to leave space for the NULL at the end.
                return false;
            }
            line[i] = '\0';
            argv[argCount] = &line[i + 1];
            argCount++;
        }
    }

    argv[argCount] = NULL;
    return true;
}

int
main(void)
{
    char  line[MAX_LINE_SIZE];
    char *argv[MAX_ARG_COUNT];

    for (;;) {
        WritePrompt();
        const unsigned lineSize = ReadLine(line, MAX_LINE_SIZE);
        if (lineSize == 0) {
            continue;
        }

        int background = 0;
        char *cmd = line;
        if (line[0] == '&') {
            background = 1;
            cmd = &line[1];
            while (*cmd == ' ' || *cmd == '\t') {
                cmd++;
            }
        }

        if (PrepareArguments(cmd, argv, MAX_ARG_COUNT) == false) {
            WriteError("too many arguments.");
            continue;
        }

        // const SpaceId newProc = Exec(cmd, !background);
        const SpaceId newProc = Exec2(cmd, !background, argv);

        if (newProc == -1) {
            WriteError("could not execute command.");
            continue;
        }

        if (!background) {
            Join(newProc);
        }
    }

    // Never reached.
    return -1;
}
