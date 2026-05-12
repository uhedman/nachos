/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "condition.hh"
#include "system.hh"


Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    lock = conditionLock;
    semaphore = new Semaphore(debugName, 0);
    waiting = 0;
}

Condition::~Condition()
{
    ASSERT(waiting == 0);
    delete semaphore;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    ASSERT(lock->IsHeldByCurrentThread());

    DEBUG('s', "Thread \"%s\" waiting on condition \"%s\"\n",
        currentThread->GetName(), name);

    waiting++;
    
    lock->Release();
    semaphore->P();
    lock->Acquire();

    DEBUG('s', "Thread \"%s\" awoken on condition \"%s\"\n",
        currentThread->GetName(), name);
}

void
Condition::Signal()
{
    ASSERT(lock->IsHeldByCurrentThread());

    DEBUG('s', "Thread \"%s\" signaling condition \"%s\" (%u waiting)\n",
        currentThread->GetName(), name, waiting);

    if (waiting > 0) {
        waiting--;
        semaphore->V();
    }
}

void
Condition::Broadcast()
{
    ASSERT(lock->IsHeldByCurrentThread());

    DEBUG('s', "Thread \"%s\" broadcasting condition \"%s\" (%u waiting)\n",
        currentThread->GetName(), name, waiting);

    while (waiting > 0)
    {
        waiting--;
        semaphore->V();
    }
}
