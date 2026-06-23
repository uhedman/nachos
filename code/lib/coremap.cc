#include "coremap.hh"

#include <stdio.h>
#include "machine/system_dep.hh"


/// Initialize a coremap with `nitems` entries, so that every entry is free.
///
/// * `nitems` is the number of entries in the coremap.
Coremap::Coremap(unsigned nitems)
{
    ASSERT(nitems > 0);

    numBits = nitems;
    map     = new CoremapEntry[numBits];
    for (unsigned i = 0; i < numBits; i++) {
        Clear(i);
    }
    clockHand = 0;
}

/// De-allocate a coremap.
Coremap::~Coremap()
{
    delete [] map;
}

/// Mark the “nth” frame as occupied.
///
/// * `which` is the number of the frame to be marked.
void
Coremap::Mark(unsigned which, unsigned vpn, AddressSpace *space)
{
    ASSERT(which < numBits);
    map[which].inUse = true;
    map[which].virtualPage = vpn;
    map[which].space = space;
}

/// Clear the “nth” frame.
///
/// * `which` is the number of the frame to be cleared.
void
Coremap::Clear(unsigned which)
{
    ASSERT(which < numBits);
    map[which].inUse = false;
    map[which].space = nullptr;
    map[which].virtualPage = 0;
}

/// Return true if the “nth” frame is occupied.
///
/// * `which` is the number of the frame to be tested.
bool
Coremap::Test(unsigned which) const
{
    ASSERT(which < numBits);
    return map[which].inUse;
}

/// Return the number of the first frame which is free.  As a side effect, mark
/// the frame as occupied.
///
/// If no frames are free, return -1.
int
Coremap::Find(unsigned vpn, AddressSpace *space)
{
    for (unsigned i = 0; i < numBits; i++) {
        if (!Test(i)) {
            Mark(i, vpn, space);
            return i;
        }
    }
    
    return -1;
}

/// Return the number of free frames in the coremap.
unsigned
Coremap::CountClear() const
{
    unsigned count = 0;

    for (unsigned i = 0; i < numBits; i++) {
        if (!Test(i)) {
            count++;
        }
    }
    return count;
}

/// Print the contents of the coremap, for debugging.
void
Coremap::Print() const
{
    printf("Coremap entries set:\n");
    for (unsigned i = 0; i < numBits; i++) {
        if (Test(i)) {
            printf("%u ", i);
        }
    }
    printf("\n");
}

/// Initialize the contents of a coremap from a Nachos file.
///
/// Note: this is not needed until the *FILESYS* assignment.
///
/// * `file` is the place to read the bitmap from.
void
Coremap::FetchFrom(OpenFile *file)
{
    ASSERT(file != nullptr);
    file->ReadAt((char *) map, numBits * sizeof (CoremapEntry), 0);
}

/// Store the contents of a coremap to a Nachos file.
///
/// Note: this is not needed until the *FILESYS* assignment.
///
/// * `file` is the place to write the bitmap to.
void
Coremap::WriteBack(OpenFile *file) const
{
    ASSERT(file != nullptr);
    file->WriteAt((char *) map, numBits * sizeof (CoremapEntry), 0);
}

/// Select a physical frame to be replaced.
#ifdef PRPOLICY_FIFO
unsigned
Coremap::PickVictim()
{
    unsigned frame = clockHand;
    clockHand = (clockHand + 1) % numBits;
    return frame;
}
#elif defined(PRPOLICY_CLOCK)
unsigned
Coremap::PickVictim()
{
    while (true) {
        for (unsigned i = 0; i < numBits; i++) {
            unsigned frame = clockHand;
            clockHand = (clockHand + 1) % numBits;

            TranslationEntry *page =
                &map[frame].space->GetPageTable()[map[frame].virtualPage];

            if (!page->use && !page->dirty) {
                return frame;
            }
        }

        for (unsigned i = 0; i < numBits; i++) {
            unsigned frame = clockHand;
            clockHand = (clockHand + 1) % numBits;

            TranslationEntry *page =
                &map[frame].space->GetPageTable()[map[frame].virtualPage];

            if (!page->use) {
                return frame;
            }
            page->use = false;
        }
    }
}
#else
unsigned
Coremap::PickVictim()
{
    return SystemDep::Random() % numBits;
}
#endif

/// Get the AddressSpace associated with a physical frame.
AddressSpace *
Coremap::GetSpace(unsigned which) const
{
    ASSERT(which < numBits);
    return map[which].space;
}

/// Get the virtual page number associated with a physical frame.
unsigned
Coremap::GetVirtualPage(unsigned which) const
{
    ASSERT(which < numBits);
    return map[which].virtualPage;
}
