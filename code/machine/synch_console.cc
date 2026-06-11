#include "synch_console.hh"
#include "threads/system.hh"

#include <stdio.h>


static void
SynchConsoleReadPoll(void *c)
{
    ASSERT(c != nullptr);
    SynchConsole *console = (SynchConsole *) c;
    console->CheckCharAvail();
}

static void
SynchConsoleWriteDone(void *c)
{
    ASSERT(c != nullptr);
    SynchConsole *console = (SynchConsole *) c;
    console->WriteDone();
}


SynchConsole::SynchConsole(const char *readFile, const char *writeFile,
        VoidFunctionPtr readAvail,
        VoidFunctionPtr writeDone, void *callArg)
{
    ASSERT(readAvail != nullptr);
    ASSERT(writeDone != nullptr);

    if (readFile == nullptr) {
        readFileNo = 0;  // keyboard = stdin
    } else {
        readFileNo = SystemDep::OpenForReadWrite(readFile, true);
    }
    // should be read-only
    if (writeFile == nullptr) {
        writeFileNo = 1;  // display = stdout
    } else {
        writeFileNo = SystemDep::OpenForWrite(writeFile);
    }

    // Set up the stuff to emulate asynchronous interrupts.
    writeHandler = writeDone;
    readHandler  = readAvail;
    handlerArg   = callArg;
    putBusy      = false;
    incoming     = EOF;

    // Start polling for incoming packets.
    interrupt->Schedule(ConsoleReadPoll, this,
                        CONSOLE_TIME, CONSOLE_READ_INT);
}

/// Clean up console emulation.
SynchConsole::~SynchConsole()
{
    delete console;
}

/// Periodically called to check if a character is available for
/// input from the simulated keyboard (eg, has it been typed?).
///
/// Only read it in if there is buffer space for it (if the previous
/// character has been grabbed out of the buffer by the Nachos kernel).
/// Invoke the “read” interrupt handler, once the character has been put into
/// the buffer.
void
SynchConsole::CheckCharAvail()
{
    char c;

    // Schedule the next time to poll for a packet.
    interrupt->Schedule(ConsoleReadPoll, this,
            CONSOLE_TIME, CONSOLE_READ_INT);

    // Do nothing if character is already buffered, or none to be read.
    if (incoming != EOF || !SystemDep::PollFile(readFileNo)) {
        return;
    }

    // Otherwise, read character and tell user about it.
    SystemDep::Read(readFileNo, &c, sizeof c);
    incoming = c;
    stats->numConsoleCharsRead++;
    (*readHandler)(handlerArg);
}

/// Internal routine called when it is time to invoke the interrupt handler
/// to tell the Nachos kernel that the output character has completed.
void
SynchConsole::WriteDone()
{
    putBusy = false;
    stats->numConsoleCharsWritten++;
    (*writeHandler)(handlerArg);
}

/// Read a character from the input buffer, if there is any there.
/// Either return the character, or EOF if none buffered.
char
SynchConsole::GetChar()
{
    char ch = incoming;

    incoming = EOF;
    return ch;
}

/// Write a character to the simulated display, schedule an interrupt to
/// occur in the future, and return.
void
SynchConsole::PutChar(char ch)
{
    ASSERT(!putBusy);
    SystemDep::WriteFile(writeFileNo, &ch, sizeof (char));
    putBusy = true;
    interrupt->Schedule(ConsoleWriteDone, this,
                        CONSOLE_TIME, CONSOLE_WRITE_INT);
}
