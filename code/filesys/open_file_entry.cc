#include "open_file_entry.hh"
#include "threads/rwlock.hh"
#include "threads/lock.hh"

OpenFileEntry::OpenFileEntry(int _sector)
{
	rwLock         = new RWLock("OpenFileEntry rwLock");
	metaLock       = new Lock("OpenFileEntry metaLock");
	openerCount    = 0;
	pendingRemoval = false;
	sector         = _sector;
}

OpenFileEntry::~OpenFileEntry()
{
	delete rwLock;
	delete metaLock;
}

void
OpenFileEntry::AcquireRead()
{
	rwLock->AcquireRead();
}

void
OpenFileEntry::ReleaseRead()
{
	rwLock->ReleaseRead();
}

void
OpenFileEntry::AcquireWrite()
{
	rwLock->AcquireWrite();
}

void
OpenFileEntry::ReleaseWrite()
{
	rwLock->ReleaseWrite();
}

void
OpenFileEntry::AddOpener()
{
	openerCount++;
}

void
OpenFileEntry::RemoveOpener()
{
	openerCount--;
}

int
OpenFileEntry::GetOpenerCount() const
{
	return openerCount;
}

void
OpenFileEntry::MarkForRemoval()
{
	pendingRemoval = true;
}

bool
OpenFileEntry::IsPendingRemoval() const
{
	return pendingRemoval;
}

int
OpenFileEntry::GetSector() const
{
	return sector;
}

void
OpenFileEntry::AcquireMetaLock()
{
	metaLock->Acquire();
}

void
OpenFileEntry::ReleaseMetaLock()
{
	metaLock->Release();
}
