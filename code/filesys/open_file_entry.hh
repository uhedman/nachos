#ifndef NACHOS_FILESYS_OPEN_FILE_ENTRY__HH
#define NACHOS_FILESYS_OPEN_FILE_ENTRY__HH

class Lock;
class RWLock;

class OpenFileEntry {
public:
    OpenFileEntry(int _sector);

    ~OpenFileEntry();

    /// Aquire lock to read file.
    void AcquireRead();

    /// Release lock after reading file.
    void ReleaseRead();

    /// Aquire lock to write to file.
    void AcquireWrite();

    /// Release lock after writing to file.
    void ReleaseWrite();

    /// Get the number of threads that have the file open.
    int GetOpenerCount() const;

    /// Add a thread to the opener count.
    void AddOpener();

    /// Remove a thread from the opener count.
    void RemoveOpener();

    /// Mark the file for removal.
    void MarkForRemoval();

    /// Check if the file is marked for removal.
    bool IsPendingRemoval() const;

    /// Get the sector of the file.
    int GetSector() const;

    /// Aquire lock for metadata operations
    void AcquireMetaLock();

    /// Release lock after metadata operations
    void ReleaseMetaLock();

private:
    /// Read-Write lock for synchronizing access to the file
    RWLock *rwLock;

    /// Lock for synchronizing access to the metadata (openerCount, pendingRemoval)
    Lock *metaLock;

    /// Number of times the file has been opened.
    int openerCount;

    /// Flag to indicate that the file is marked for removal.
    bool pendingRemoval;

    /// Sector of the file.
    int sector;
};

#endif
