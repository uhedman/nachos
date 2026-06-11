/// Routines to synchronously access the console device.  The raw console is
/// asynchronous (reads/writes return immediately and an interrupt fires
/// later).  This is a layer on top providing a synchronous interface
/// (requests block until they complete).
///
/// Use semaphores to synchronize the interrupt handlers with the pending
/// requests.  And, because the console can only handle one output operation
/// at a time, use a lock to enforce mutual exclusion on writes.
///
/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "synch_console.hh"


/// Dummy functions because C++ is weird about pointers to member functions.

static void
SynchConsoleReadAvail(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *console = (SynchConsole *) arg;
    console->ReadAvail();
}

static void
SynchConsoleWriteDone(void *arg)
{
    ASSERT(arg != nullptr);
    SynchConsole *console = (SynchConsole *) arg;
    console->WriteDone();
}

SynchConsole::SynchConsole(const char *readFile, const char *writeFile)
{
    readAvail  = new Semaphore("synch console read",  0);
    writeDone  = new Semaphore("synch console write", 0);
    writeLock  = new Lock("synch console write lock");
    console    = new Console(readFile, writeFile,
                             SynchConsoleReadAvail, SynchConsoleWriteDone, this);
}

SynchConsole::~SynchConsole()
{
    delete console;
    delete writeLock;
    delete writeDone;
    delete readAvail;
}

char
SynchConsole::GetChar()
{
    readAvail->P();
    return console->GetChar();
}

void
SynchConsole::PutChar(char ch)
{
    writeLock->Acquire();

    console->PutChar(ch);
    writeDone->P();

    writeLock->Release();
}

void
SynchConsole::ReadAvail()
{
    readAvail->V();
}

void
SynchConsole::WriteDone()
{
    writeDone->V();
}