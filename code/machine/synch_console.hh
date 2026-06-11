#ifndef NACHOS_MACHINE_SYNCHCONSOLE__HH
#define NACHOS_MACHINE_SYNCHCONSOLE__HH


#include "console.hh"
#include "threads/lock.hh"
#include "threads/semaphore.hh"


class SynchConsole {
public:

    /// Initialize a synchronous console, by initializing the raw Console.
    SynchConsole(const char *readFile, const char *writeFile);

    /// De-allocate the synch console data.
    ~SynchConsole();

    /// Read one character from the console, blocking until one is available.
    char GetChar();

    /// Write one character to the console, returning only once it has been
    /// sent.
    void PutChar(char ch);

    /// Called by the console interrupt handlers; do not call directly.
    void ReadAvail();
    void WriteDone();

private:
    Console   *console;       ///< Raw console device.
    Semaphore *readAvail;     ///< Signalled when a character arrives.
    Semaphore *writeDone;     ///< Signalled when a write completes.
    Lock      *writeLock;     ///< Only one `PutChar` at a time.
};


#endif