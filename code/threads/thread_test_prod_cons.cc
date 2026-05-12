/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "lock.hh"
#include "condition.hh"

#include <stdio.h>

const static int NUM_ITEMS = 1000;
static int buffer[3] = {0};
static int items = 0;
static Lock *bufferLock = new Lock("BufferLock");
static Condition *conditionOccupied = new Condition("Buffer con items", bufferLock);
static Condition *conditionFree = new Condition("Buffer con espacio", bufferLock);
static bool producerDone = false;

void
Producer(void *_arg) {
    for (int i = 1; i < NUM_ITEMS + 1; i++)
    {
        bufferLock->Acquire();

        while (items == 3) 
        {
            printf("Productor esperando (buffer lleno)\n");
            conditionFree->Wait();
        }
        
        printf("Productor produce: %d en %d\n", i, items);
        buffer[items] = i;
        items++;
        conditionOccupied->Signal();

        bufferLock->Release();

        currentThread->Yield();
    }

    producerDone = true;
}

void
Consumer(void *_arg) {
    for (int i = 0; i < NUM_ITEMS; i++)
    {
        bufferLock->Acquire();

        while (items == 0)
        {
            printf("Consumidor esperando (buffer vacio)\n"); 
            conditionOccupied->Wait();
        }
        
        int item = buffer[items - 1];
        buffer[items - 1] = 0;
        printf("Consumidor consume: %d en %d\n", item, items - 1);
        items--;
        conditionFree->Signal();

        bufferLock->Release();

        currentThread->Yield();
    }
}

void
ThreadTestProdCons()
{
    Thread *consumer = new Thread("Consumer");

    consumer->Fork(Consumer, NULL);
    Producer(NULL);
    
    while (!producerDone)
    {
        currentThread->Yield();
    }
    
    // Free space
    delete bufferLock;
    delete conditionOccupied;
    delete conditionFree;
}
