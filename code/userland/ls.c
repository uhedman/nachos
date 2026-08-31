/// List all files in directory.

#include "syscall.h"
#include "lib.c"

#define USAGE       "Usage: ls [<directory> ...]"
#define LIST_ERROR  "Error: could not list directory."

int
main(int argc, char *argv[])
{
	if (argc < 1) {
		PrintString(USAGE);
		Exit(1);
	}

	if (argc == 1) {
		if (List(".") < 0) {
			PrintString(LIST_ERROR);
			return -1;
		}

		return 0;
	}

	int error = 0;
	for (unsigned i = 1; i < argc; i++) {
		if (List(argv[i]) < 0) {
			PrintString(LIST_ERROR);
			error = 1;
		}
	}

	return error;
}
