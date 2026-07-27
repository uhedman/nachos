/// Routines for the readers-writer lock (RWLock) synchronization primitive.
///
/// Implements a *writer-preferred* readers-writer lock on top of the
/// existing `Lock` and `Condition` Nachos primitives.
///
/// Copyright (c) 2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "rwlock.hh"


RWLock::RWLock(const char *debugName)
{
    name          = debugName;
    lock          = new Lock(debugName);
    cond          = new Condition(debugName, lock);
    activeReaders = 0;
    activeWriter  = false;
    waitingWriters = 0;
}

RWLock::~RWLock()
{
    delete cond;
    delete lock;
}

const char *
RWLock::GetName() const
{
    return name;
}

/// Block until no writer is active or pending, then increment the reader
/// count and return.
void
RWLock::AcquireRead()
{
    lock->Acquire();
    while (waitingWriters > 0 || activeWriter) {
        cond->Wait();
    }
    activeReaders++;
    lock->Release();
}

/// Decrement the reader count.  If this was the last reader, broadcast to
/// wake any waiting writers.
void
RWLock::ReleaseRead()
{
    lock->Acquire();
    activeReaders--;
    if (activeReaders == 0) {
        cond->Broadcast();
    }
    lock->Release();
}

/// Register as a waiting writer, then block until no readers are active
/// and no other writer holds the lock.
void
RWLock::AcquireWrite()
{
    lock->Acquire();
    waitingWriters++;
    while (activeReaders > 0 || activeWriter) {
        cond->Wait();
    }
    waitingWriters--;
    activeWriter = true;
    lock->Release();
}

/// Release the write lock and broadcast to wake all waiters.
void
RWLock::ReleaseWrite()
{
    lock->Acquire();
    activeWriter = false;
    cond->Broadcast();
    lock->Release();
}
