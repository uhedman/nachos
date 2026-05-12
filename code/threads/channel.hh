#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "lock.hh"
#include "semaphore.hh"

class Channel {
public:

    Channel(const char *debugName);

    ~Channel();

    /// For debugging.
    const char *GetName() const;

    void Send(int message);
    void Receive(int *message);

private:

    /// For debugging.
    const char *name;


    int buffer;
    Semaphore *senderSem;
    Semaphore *receiverSem;
    Semaphore *finishedSem;
};


#endif
