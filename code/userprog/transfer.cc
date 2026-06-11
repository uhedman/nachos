/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"


void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp;

        ASSERT(machine->ReadMem(userAddress++, 1, &temp));
        *outBuffer++ = (unsigned char) temp;

        count++;
    } while (count < byteCount);

    return;
}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount)
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        
        ASSERT(machine->ReadMem(userAddress++, 1, &temp));
        *outString = (unsigned char) temp;

        count++;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount)
{
    ASSERT(buffer != nullptr);
    ASSERT(userAddress != 0);
    ASSERT(byteCount != 0);

    unsigned count = 0;
    do {
        int temp = (unsigned char) *buffer;

        ASSERT(machine->WriteMem(userAddress++, 1, temp));

        count++;
        buffer++;
    } while (count < byteCount);
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(string != nullptr);
    ASSERT(userAddress != 0);

    do {
        int temp = (unsigned char) *string;

        ASSERT(machine->WriteMem(userAddress++, 1, temp));
    } while (*string++ != '\0');

    return;
}
