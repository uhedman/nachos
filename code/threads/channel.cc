#include "channel.hh"


Channel::Channel(const char *debugName)
{
    name = debugName;
    senderSem = new Semaphore("Sender semaphore", 1);
    receiverSem = new Semaphore("Receiver semaphore", 0);
    finishedSem = new Semaphore("Finished semaphore", 0);
}

Channel::~Channel()
{
    delete senderSem;
    delete receiverSem;
    delete finishedSem;
}

const char *
Channel::GetName() const
{
    return name;
}

void
Channel::Send(int message)
{
    senderSem->P();

    buffer = message;
    receiverSem->V();
    finishedSem->P();

    senderSem->V();
}

void
Channel::Receive(int *message)
{
    receiverSem->P();

    *message = buffer;

    finishedSem->V();
}
