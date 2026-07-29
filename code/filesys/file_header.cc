/// Routines for managing the disk file header (in UNIX, this would be called
/// the i-node).
///
/// The file header is used to locate where on disk the file's data is
/// stored.  We implement this as a fixed size table of pointers -- each
/// entry in the table points to the disk sector containing that portion of
/// the file data (in other words, there are no indirect or doubly indirect
/// blocks). The table size is chosen so that the file header will be just
/// big enough to fit in one disk sector,
///
/// Unlike in a real system, we do not keep track of file permissions,
/// ownership, last modification date, etc., in the file header.
///
/// A file header can be initialized in two ways:
///
/// * for a new file, by modifying the in-memory data structure to point to
///   the newly allocated data blocks;
/// * for a file already on disk, by reading the file header from disk.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_header.hh"
#include "threads/system.hh"

#include <ctype.h>
#include <stdio.h>


/// Initialize a fresh file header for a newly created file.  Allocate data
/// blocks for the file out of the map of free disk blocks.  Return false if
/// there are not enough free blocks to accomodate the new file.
///
/// * `freeMap` is the bit map of free disk sectors.
/// * `fileSize` is the size of the file in bytes.
bool
FileHeader::Allocate(Bitmap *freeMap, unsigned fileSize)
{
	ASSERT(freeMap != nullptr);

	if (fileSize > MAX_FILE_SIZE) {
		return false;
	}

	DEBUG('f', "Allocating file of size %u\n", fileSize);

	raw.numBytes = fileSize;
	raw.numSectors = DivRoundUp(fileSize, SECTOR_SIZE);

	unsigned totalSectorsNeeded = raw.numSectors;
	if (raw.numSectors > NUM_DIRECT) {
		totalSectorsNeeded++; // One sector for the table pointed to by single indirect
	}
	if (raw.numSectors > NUM_DIRECT + POINTERS_PER_SECTOR) {
		totalSectorsNeeded++; // One sector for the table pointed to by double indirect
		unsigned remaining = raw.numSectors - NUM_DIRECT - POINTERS_PER_SECTOR;
		// One sector for each table pointed to by each single indirect inside double
		totalSectorsNeeded += DivRoundUp(remaining, POINTERS_PER_SECTOR);
	}

	if (freeMap->CountClear() < totalSectorsNeeded) {
		return false;
	}

	// Allocate direct sectors
	unsigned directSectors = raw.numSectors < NUM_DIRECT ? raw.numSectors : NUM_DIRECT;
	for (unsigned i = 0; i < directSectors; i++) {
		raw.dataSectors[i] = freeMap->Find();
	}

	// Allocate single indirect sectors
	if (raw.numSectors > NUM_DIRECT) {
		raw.singleIndirect = freeMap->Find();
		unsigned *singleIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
		unsigned indirectSectors = raw.numSectors - NUM_DIRECT;

		if (indirectSectors > POINTERS_PER_SECTOR) {
			indirectSectors = POINTERS_PER_SECTOR;
		}
		for (unsigned i = 0; i < indirectSectors; i++) {
			singleIndirectBlock[i] = freeMap->Find();
		}

		synchDisk->WriteSector(raw.singleIndirect, (char *)singleIndirectBlock);
		delete[] singleIndirectBlock;
	}

	// Allocate double indirect sectors
	if (raw.numSectors > NUM_DIRECT + POINTERS_PER_SECTOR) {
		raw.doubleIndirect = freeMap->Find();
		unsigned *doubleIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
		
		unsigned remaining = raw.numSectors - NUM_DIRECT - POINTERS_PER_SECTOR;
		unsigned innerBlocks = DivRoundUp(remaining, POINTERS_PER_SECTOR);

		// Allocate each inner single indirect block
		for (unsigned i = 0; i < innerBlocks; i++) {
			doubleIndirectBlock[i] = freeMap->Find();
			unsigned *innerIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
			
			unsigned dataInThisBlock = remaining - (i * POINTERS_PER_SECTOR);
			if (dataInThisBlock > POINTERS_PER_SECTOR) {
				dataInThisBlock = POINTERS_PER_SECTOR;
			}
			
			for (unsigned j = 0; j < dataInThisBlock; j++) {
				innerIndirectBlock[j] = freeMap->Find();
			}

			synchDisk->WriteSector(doubleIndirectBlock[i], (char *)innerIndirectBlock);
			delete[] innerIndirectBlock;
		}

		synchDisk->WriteSector(raw.doubleIndirect, (char *)doubleIndirectBlock);
		delete[] doubleIndirectBlock;
	}

	return true;
}

/// De-allocate all the space allocated for data blocks for this file.
///
/// * `freeMap` is the bit map of free disk sectors.
void
FileHeader::Deallocate(Bitmap *freeMap)
{
	ASSERT(freeMap != nullptr);

	// Deallocate direct sectors
	unsigned directSectors = raw.numSectors < NUM_DIRECT ? raw.numSectors : NUM_DIRECT;
	for (unsigned i = 0; i < directSectors; i++) {
		ASSERT(freeMap->Test(raw.dataSectors[i]));
		freeMap->Clear(raw.dataSectors[i]);
	}

	// Deallocate single indirect sector
	if (raw.numSectors > NUM_DIRECT) {
		unsigned *singleIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
		synchDisk->ReadSector(raw.singleIndirect, (char *)singleIndirectBlock);
		
		unsigned indirectSectors = raw.numSectors - NUM_DIRECT;
		if (indirectSectors > POINTERS_PER_SECTOR) {
			indirectSectors = POINTERS_PER_SECTOR;
		}
		for (unsigned i = 0; i < indirectSectors; i++) {
			ASSERT(freeMap->Test(singleIndirectBlock[i]));
			freeMap->Clear(singleIndirectBlock[i]);
		}

		ASSERT(freeMap->Test(raw.singleIndirect));
		freeMap->Clear(raw.singleIndirect);
		delete [] singleIndirectBlock;
	}

	// Deallocate double indirect sector
	if (raw.numSectors > NUM_DIRECT + POINTERS_PER_SECTOR) {
		unsigned *doubleIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
		synchDisk->ReadSector(raw.doubleIndirect, (char *)doubleIndirectBlock);

		unsigned remaining = raw.numSectors - NUM_DIRECT - POINTERS_PER_SECTOR;
		unsigned innerBlocks = DivRoundUp(remaining, POINTERS_PER_SECTOR);

		// Deallocate each inner single indirect block
		for (unsigned i = 0; i < innerBlocks; i++) {
			unsigned *innerIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
			synchDisk->ReadSector(doubleIndirectBlock[i], (char *)innerIndirectBlock);

			unsigned dataInThisBlock = remaining - (i * POINTERS_PER_SECTOR);
			if (dataInThisBlock > POINTERS_PER_SECTOR) {
				dataInThisBlock = POINTERS_PER_SECTOR;
			}

			for (unsigned j = 0; j < dataInThisBlock; j++) {
				ASSERT(freeMap->Test(innerIndirectBlock[j]));
				freeMap->Clear(innerIndirectBlock[j]);
			}

			ASSERT(freeMap->Test(doubleIndirectBlock[i]));
			freeMap->Clear(doubleIndirectBlock[i]);
			delete[] innerIndirectBlock;
		}

		ASSERT(freeMap->Test(raw.doubleIndirect));
		freeMap->Clear(raw.doubleIndirect);
		delete[] doubleIndirectBlock;
	}
}

/// Fetch contents of file header from disk.
///
/// * `sector` is the disk sector containing the file header.
void
FileHeader::FetchFrom(unsigned sector)
{
	synchDisk->ReadSector(sector, (char *) &raw);
}

/// Write the modified contents of the file header back to disk.
///
/// * `sector` is the disk sector to contain the file header.
void
FileHeader::WriteBack(unsigned sector)
{
	synchDisk->WriteSector(sector, (char *) &raw);
}

/// Return which disk sector is storing a particular byte within the file.
/// This is essentially a translation from a virtual address (the offset in
/// the file) to a physical address (the sector where the data at the offset
/// is stored).
///
/// * `offset` is the location within the file of the byte in question.
unsigned
FileHeader::ByteToSector(unsigned offset)
{
	unsigned logicalSector = offset / SECTOR_SIZE;
	if (logicalSector < NUM_DIRECT) {
		return raw.dataSectors[logicalSector];
	} else if (NUM_DIRECT <= logicalSector && logicalSector < NUM_DIRECT + POINTERS_PER_SECTOR ) {
		unsigned index = logicalSector - NUM_DIRECT;

		unsigned *indirectBlock = new unsigned[POINTERS_PER_SECTOR];
		synchDisk->ReadSector(raw.singleIndirect, (char *)indirectBlock);

		unsigned result = indirectBlock[index];
		delete[] indirectBlock;
		return result;
	} else {
		unsigned sectors = logicalSector - NUM_DIRECT - POINTERS_PER_SECTOR;
		unsigned indirectIndex = sectors / POINTERS_PER_SECTOR;
		unsigned index = sectors % POINTERS_PER_SECTOR;

		unsigned *doubleIndirectBlock = new unsigned[POINTERS_PER_SECTOR];
		synchDisk->ReadSector(raw.doubleIndirect, (char *)doubleIndirectBlock);


		unsigned *indirectBlock = new unsigned[POINTERS_PER_SECTOR];
		synchDisk->ReadSector(doubleIndirectBlock[indirectIndex], (char *)indirectBlock);

		unsigned result = indirectBlock[index];
		delete[] indirectBlock;
		delete[] doubleIndirectBlock;
		return result;
	}
}

/// Return the number of bytes in the file.
unsigned
FileHeader::FileLength() const
{
	return raw.numBytes;
}

/// Print the contents of the file header, and the contents of all the data
/// blocks pointed to by the file header.
void
FileHeader::Print(const char *title)
{
	char *data = new char [SECTOR_SIZE];

	if (title == nullptr) {
		printf("File header:\n");
	} else {
		printf("%s file header:\n", title);
	}

	printf("    size: %u bytes\n"
		   "    block indexes: ",
		   raw.numBytes);

	for (unsigned i = 0; i < raw.numSectors; i++) {
		printf("%u ", ByteToSector(i * SECTOR_SIZE));
	}
	printf("\n");

	for (unsigned i = 0, k = 0; i < raw.numSectors; i++) {
		unsigned sector = ByteToSector(i * SECTOR_SIZE);
		printf("    contents of block %u:\n", sector);
		synchDisk->ReadSector(sector, data);
		for (unsigned j = 0; j < SECTOR_SIZE && k < raw.numBytes; j++, k++) {
			if (isprint(data[j])) {
				printf("%c", data[j]);
			} else {
				printf("\\%X", (unsigned char) data[j]);
			}
		}
		printf("\n");
	}
	delete [] data;
}

const RawFileHeader *
FileHeader::GetRaw() const
{
	return &raw;
}
