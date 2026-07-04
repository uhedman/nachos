#ifndef NACHOS_LIB_COREMAP__HH
#define NACHOS_LIB_COREMAP__HH


#include "utility.hh"
#include "filesys/open_file.hh"


class AddressSpace;

struct CoremapEntry {
    /// Whether this frame is in use.
    bool inUse;

    /// The AddressSpace that is using this frame, if any.
    AddressSpace *space;

    /// The virtual page number that is using this frame, if any.
    unsigned virtualPage;
};

/// A “coremap” -- an array of entries, each of which can be independently set,
/// cleared, and tested.
///
/// Most useful for managing the allocation of the elements of an array --
/// for instance, disk sectors, or main memory pages.
///
/// Each entry represents whether the corresponding sector or page is in use
/// or free.
class Coremap {
public:

    /// Initialize a coremap with `nitems` entries; all entries are free.
    ///
    /// * `nitems` is the number of items in the coremap.
    Coremap(unsigned nitems);

    /// Uninitialize a coremap.
    ~Coremap();

    /// Set the “nth” entry.
    void Mark(unsigned which, unsigned vpn, AddressSpace *space);

    /// Clear the “nth” entry.
    void Clear(unsigned which);

    /// Is the “nth” entry set?
    bool Test(unsigned which) const;

    /// Return the index of a free entry, and as a side effect, set it.
    ///
    /// If no entries are free, return -1.
    int Find(unsigned vpn, AddressSpace *space);

    /// Return the number of clear entries.
    unsigned CountClear() const;

    /// Print contents of coremap.
    void Print() const;

    /// Fetch contents from disk.
    void FetchFrom(OpenFile *file);

    /// Write contents to disk.
    void WriteBack(OpenFile *file) const;

    /// Select a physical frame to be replaced.
    unsigned PickVictim();

    /// Get the AddressSpace associated with a physical frame.
    AddressSpace *GetSpace(unsigned which) const;

    /// Get the virtual page number associated with a physical frame.
    unsigned GetVirtualPage(unsigned which) const;

private:

    /// Number of entries in the coremap.
    unsigned numBits;

    /// Entries storage.
    CoremapEntry *map;

    /// Hand position for the clock page replacement algorithm.
    unsigned clockHand;
};


#endif
