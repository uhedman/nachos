/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_simple.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>

#ifdef SEMAPHORE_TEST
#include "semaphore.hh"
Semaphore* semaphore = new Semaphore("Ejercicio 15", 3);
#endif

/// Loop 10 times, yielding the CPU to another ready thread each iteration.
///
/// * `name` points to a string with a thread name, just for debugging
///   purposes.

const unsigned NUM_THREADS = 4;
bool threadNDone[NUM_THREADS] = {false};

void
SimpleThread(void *name_)
{

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    for (unsigned num = 0; num < 10; num++) {
        
        #ifdef SEMAPHORE_TEST
        semaphore->P();
        #endif

        printf("*** Thread `%s` is running: iteration %u\n", currentThread->GetName(), num);

        #ifdef SEMAPHORE_TEST
        semaphore->V();
        #endif

        currentThread->Yield();
    }
    
    // Get threadId from currentThread name
    unsigned threadId;
    if (sscanf(currentThread->GetName(), "Thread-%u", &threadId) == 1) {
        threadNDone[threadId] = true;
    }

    printf("!!! Thread `%s` has finished SimpleThread\n", currentThread->GetName());
}

/// Check all threads are done
bool 
AllThreadsDone() {
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        if (!threadNDone[i]) return false;
    }
    return true;
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching one thread which calls `SimpleThread`, and finally
/// calling `SimpleThread` on the current thread.
void
ThreadTestSimple()
{
    Thread **newThreads = new Thread*[NUM_THREADS];
    char **threadNames = new char*[32];

    // Generate threads and their names
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        threadNames[i] = new char[32];
        snprintf(threadNames[i], 32, "Thread-%u", i);
        newThreads[i] = new Thread(threadNames[i]);
        newThreads[i]->Fork(SimpleThread, (void*)threadNames[i]);
    }

    //the "main" thread also executes the same function
    SimpleThread(NULL);

    //Wait for the threads to finish if needed
    while (!AllThreadsDone()) {
        currentThread->Yield(); 
    }

    printf("Test finished\n");

    // Free memory
    delete [] newThreads;
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        delete [] threadNames[i];
    }
    delete [] threadNames;
}
