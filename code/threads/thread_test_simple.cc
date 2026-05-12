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

void
SimpleThread(void *name_)
{

    // If the lines dealing with interrupts are commented, the code will
    // behave incorrectly, because printf execution may cause race
    // conditions.
    for (unsigned num = 0; num < 10; num++) {
        
        #ifdef SEMAPHORE_TEST
        semaphore->P();
        DEBUG('s', "*** Thread `%s` acquired the semaphore %s\n", currentThread->GetName(), semaphore->GetName());
        #endif

        printf("*** Thread `%s` is running: iteration %u\n", currentThread->GetName(), num);

        #ifdef SEMAPHORE_TEST
        semaphore->V();
        DEBUG('s', "*** Thread `%s` released the semaphore %s\n", currentThread->GetName(), semaphore->GetName());
        #endif

        currentThread->Yield();
    }

    printf("!!! Thread `%s` has finished SimpleThread\n", currentThread->GetName());
}

/// Set up a ping-pong between several threads.
///
/// Do it by launching one thread which calls `SimpleThread`, and finally
/// calling `SimpleThread` on the current thread.
void
ThreadTestSimple()
{
    char **names = new char*[NUM_THREADS];
    Thread **threads = new Thread*[NUM_THREADS];

    // Generate threads and their names
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        names[i] = new char[16];
        sprintf(names[i], "Thread-%u", i);
        threads[i] = new Thread(names[i], true);
        threads[i]->Fork(SimpleThread, (void*)names[i]);
    }

    //the "main" thread also executes the same function
    SimpleThread(NULL);

    //Wait for the threads to finish if needed
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        threads[i]->Join();
    }

    printf("Test finished\n");

    // Free memory
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        delete [] names[i];
    }
    delete [] names;
    delete [] threads;
}
