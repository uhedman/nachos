/// Readers-writer lock (RWLock), a synchronization primitive.
///
/// A data structure for synchronizing multiple concurrent readers with
/// exclusive writers.  Implements a *writer-preferred* policy: once a
/// writer is waiting, new readers are blocked until the writer finishes,
/// preventing writer starvation.
///
/// Supported operations:
/// * `AcquireRead`  -- wait until no writer is active or pending, then
///   enter as a concurrent reader.
/// * `ReleaseRead`  -- leave as a reader; broadcast to wake waiting writers
///   if this is the last reader.
/// * `AcquireWrite` -- wait until no readers are active and no other
///   writer is active, then enter exclusively.
/// * `ReleaseWrite` -- leave as the writer and wake all waiters.
///
/// Copyright (c) 2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_THREADS_RWLOCK__HH
#define NACHOS_THREADS_RWLOCK__HH

#include "lock.hh"
#include "condition.hh"


class RWLock {
public:

    /// Constructor: initialise the lock in the unlocked state.
    RWLock(const char *debugName);

    ~RWLock();

    /// For debugging.
    const char *GetName() const;

    /// Acquire a shared read lock.
    ///
    /// Blocks while any writer is active or waiting (writer-preferred).
    void AcquireRead();

    /// Release a previously acquired read lock.
    void ReleaseRead();

    /// Acquire an exclusive write lock.
    ///
    /// Blocks while any reader is active or another writer holds the lock.
    void AcquireWrite();

    /// Release a previously acquired write lock.
    void ReleaseWrite();

private:

    const char *name;

    /// Internal mutex protecting all counter fields.
    Lock      *lock;

    /// Condition broadcasted whenever state changes (reader leaves or
    /// writer leaves).
    Condition *cond;

    /// Number of threads currently holding a read lock.
    int        activeReaders;

    /// Whether a writer is currently holding the write lock.
    bool       activeWriter;

    /// Number of writers blocked waiting to acquire the write lock.
    /// Used to implement the writer-preferred policy.
    int        waitingWriters;
};


#endif
