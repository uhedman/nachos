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


#include "lock.hh"
#include "system.hh"


Lock::Lock(const char *debugName)
{
    name = debugName;
    semaphore = new Semaphore(debugName, 1);
    currentHolder = nullptr;
}

Lock::~Lock()
{
    ASSERT(currentHolder == nullptr);
    delete semaphore;
}

const char *
Lock::GetName() const
{
    return name;
}

void
Lock::Acquire()
{
    DEBUG('s', "Thread \"%s\" requested acquisition of lock \"%s\"\n", 
        currentThread->GetName(), this->GetName());

    ASSERT(!IsHeldByCurrentThread());
    semaphore->P();
    currentHolder = currentThread;

    DEBUG('s', "Thread \"%s\" acquired lock \"%s\"\n", 
        currentThread->GetName(), this->GetName());
}

void
Lock::Release()
{
    DEBUG('s', "Thread \"%s\" requested release of lock \"%s\"\n", 
        currentThread->GetName(), this->GetName());

    ASSERT(IsHeldByCurrentThread());
    currentHolder = nullptr;
    semaphore->V();

    DEBUG('s', "Thread \"%s\" released lock \"%s\"\n", 
        currentThread->GetName(), this->GetName());
}

bool
Lock::IsHeldByCurrentThread() const
{
    return currentThread == currentHolder;
}
