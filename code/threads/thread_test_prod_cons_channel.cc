/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons_channel.hh"
#include "system.hh"
#include "channel.hh"

#include <stdio.h>

const static int NUM_ITEMS = 1000;
static Channel *channel = new Channel("Channel");

void
ProducerChannel(void *_arg) {
    for (int i = 1; i < NUM_ITEMS + 1; i++)
    {
        printf("Productor produce: %d en %d\n", i, i);
        channel->Send(i);
        currentThread->Yield();
    }
}

void
ConsumerChannel(void *_arg) {
    for (int i = 1; i < NUM_ITEMS + 1; i++)
    {
        int message;
        channel->Receive(&message);
        printf("Consumidor consume: %d en %d\n", message, i);

        currentThread->Yield();
    }
}

void
ThreadTestProdConsChannel()
{
    Thread *consumer = new Thread("Consumer", true);
    Thread *producer = new Thread("Producer", true);

    consumer->Fork(ConsumerChannel, NULL);
    producer->Fork(ProducerChannel, NULL);
    
    consumer->Join();
    producer->Join();
    
    // Free space
    delete channel;
}
