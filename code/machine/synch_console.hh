#ifndef NACHOS_MACHINE_SYNCH_CONSOLE__HH
#define NACHOS_MACHINE_SYNCH_CONSOLE__HH


#include "lib/utility.hh"


class SynchConsole {
public:
    SynchConsole(const char *readFile, const char *writeFile,
                VoidFunctionPtr readAvail, VoidFunctionPtr writeDone,
                void *callArg);

    ~SynchConsole();

    void PutChar(char ch);
    char GetChar();

    void WriteDone();
    void CheckCharAvail();

private:
    Console *console;
    Semaphore *writerSemaphore;  ///< To wait for a write to finish.
    Semaphore *readerSemaphore;  ///< To wait for a read to finish.
    Lock *lock;
};


#endif  // SYNCH_CONSOLE_H
