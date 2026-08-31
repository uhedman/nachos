/// Routines to manage the overall operation of the file system.  Implements
/// routines to map from textual file names to files.
///
/// Each file in the file system has:
/// * a file header, stored in a sector on disk (the size of the file header
///   data structure is arranged to be precisely the size of 1 disk sector);
/// * a number of data blocks;
/// * an entry in the file system directory.
///
/// The file system consists of several data structures:
/// * A bitmap of free disk sectors (cf. `bitmap.h`).
/// * A directory of file names and file headers.
///
/// Both the bitmap and the directory are represented as normal files.  Their
/// file headers are located in specific sectors (sector 0 and sector 1), so
/// that the file system can find them on bootup.
///
/// The file system assumes that the bitmap and directory files are kept
/// “open” continuously while Nachos is running.
///
/// For those operations (such as `Create`, `Remove`) that modify the
/// directory and/or bitmap, if the operation succeeds, the changes are
/// written immediately back to disk (the two files are kept open during all
/// this time).  If the operation fails, and we have modified part of the
/// directory and/or bitmap, we simply discard the changed version, without
/// writing it back to disk.
///
/// Our implementation at this point has the following restrictions:
///
/// * there is no synchronization for concurrent accesses;
/// * files have a fixed size, set when the file is created;
/// * files cannot be bigger than about 3KB in size;
/// * there is no hierarchical directory structure, and only a limited number
///   of files can be added to the system;
/// * there is no attempt to make the system robust to failures (if Nachos
///   exits in the middle of an operation that modifies the file system, it
///   may corrupt the disk).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "file_system.hh"
#include "threads/lock.hh"
#include "directory.hh"
#include "file_header.hh"
#include "lib/bitmap.hh"

#include <stdio.h>
#include <string.h>


/// Sectors containing the file headers for the bitmap of free sectors, and
/// the directory of files.  These file headers are placed in well-known
/// sectors, so that they can be located on boot-up.
static const unsigned FREE_MAP_SECTOR = 0;
static const unsigned DIRECTORY_SECTOR = 1;

/// Initialize the file system.  If `format == true`, the disk has nothing on
/// it, and we need to initialize the disk to contain an empty directory, and
/// a bitmap of free sectors (with almost but not all of the sectors marked
/// as free).
///
/// If `format == false`, we just have to open the files representing the
/// bitmap and the directory.
///
/// * `format` -- should we initialize the disk?
FileSystem::FileSystem(bool format)
{
    DEBUG('f', "Initializing the file system.\n");
    openFileTable = new Table<OpenFileEntry*>();
    directoryLock = new Lock("Directory Lock");
    freeMapLock = new Lock("FreeMap Lock");
    openFileTableLock = new Lock("OpenFileTable Lock");

    if (format) {
        Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
        Directory  *dir     = new Directory(NUM_DIR_ENTRIES);
        FileHeader *mapH    = new FileHeader;
        FileHeader *dirH    = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FREE_MAP_SECTOR);
        freeMap->Mark(DIRECTORY_SECTOR);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapH->Allocate(freeMap, FREE_MAP_FILE_SIZE));
        ASSERT(dirH->Allocate(freeMap, DIRECTORY_FILE_SIZE));
        dirH->SetDirectory(true);

        // Flush the bitmap and directory `FileHeader`s back to disk.
        // We need to do this before we can `Open` the file, since open reads
        // the file header off of disk (and currently the disk has garbage on
        // it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapH->WriteBack(FREE_MAP_SECTOR);
        dirH->WriteBack(DIRECTORY_SECTOR);

        // OK to open the bitmap and directory files now.
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(new OpenFileEntry(FREE_MAP_SECTOR));        
        directoryFile = new OpenFile(new OpenFileEntry(DIRECTORY_SECTOR));

        // Once we have the files “open”, we can write the initial version of
        // each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile);     // flush changes to disk
        dir->WriteBack(directoryFile);

        if (debug.IsEnabled('f')) {
            freeMap->Print();
            dir->Print();

            delete freeMap;
            delete dir;
            delete mapH;
            delete dirH;
        }
    } else {
        // If we are not formatting the disk, just open the files
        // representing the bitmap and directory; these are left open while
        // Nachos is running.
        freeMapFile = new OpenFile(new OpenFileEntry(FREE_MAP_SECTOR));
        directoryFile = new OpenFile(new OpenFileEntry(DIRECTORY_SECTOR));
    }
}

FileSystem::~FileSystem()
{
    delete freeMapFile;
    delete directoryFile;
    delete openFileTable;
    delete directoryLock;
    delete freeMapLock;
    delete openFileTableLock;
}

/// Create a file in the Nachos file system (similar to UNIX `create`).
/// Since we cannot increase the size of files dynamically, we have to give
/// `Create` the initial size of the file.
///
/// The steps to create a file are:
/// 1. Make sure the file does not already exist.
/// 2. Allocate a sector for the file header.
/// 3. Allocate space on disk for the data blocks for the file.
/// 4. Add the name to the directory.
/// 5. Store the new file header on disk.
/// 6. Flush the changes to the bitmap and the directory back to disk.
///
/// Return true if everything goes ok, otherwise, return false.
///
/// Create fails if:
/// * file is already in directory;
/// * no free space for file header;
/// * no free entry for file in directory;
/// * no free space for data blocks for the file.
///
/// Note that this implementation assumes there is no concurrent access to
/// the file system!
///
/// * `name` is the name of file to be created.
bool
FileSystem::Create(const char *name) // TODO
{
    ASSERT(name != nullptr);

    DEBUG('f', "Creating file %s\n", name);

    PathResolution resolution;
    bool resolutionSuccess = ResolvePath(name, &resolution);

    if (!resolutionSuccess) {
        DEBUG('f', "File %s cannot be created: invalid path\n", name);
        return false;
    }

    if (strcmp(resolution.fileName, "") == 0) {
        DEBUG('f', "File %s cannot be created: invalid name\n", name);
        delete[] resolution.fileName;
        return false;
    }

    char *fileName = resolution.fileName;
    int parentDirSector = resolution.parentDirSector;

    OpenFile *parentDirFile = new OpenFile(new OpenFileEntry(parentDirSector));
    Directory *parentDir = new Directory(NUM_DIR_ENTRIES);
    parentDir->FetchFrom(parentDirFile);

    bool success;

    if (parentDir->Find(fileName) != -1) {
        DEBUG('f', "File %s already exists\n", fileName);
        success = false;
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);

        freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);
        
        int sector = freeMap->Find(); // Buscar sector para el FileHeader
        
        if (sector == -1) {
            DEBUG('f', "File %s cannot be created: no free space for file header\n", fileName);
            success = false;
            freeMapLock->Release();
        } else if (!parentDir->Add(fileName, sector)){
            DEBUG('f', "File %s cannot be created: no free space in directory\n", fileName);
            success = false;
            freeMapLock->Release();
        } else {
            FileHeader *h = new FileHeader;
            
            // Allocate asigna bloques de datos (en este caso 0, pero es buena práctica)
            // mientras aún tenemos el lock del freeMap
            success = h->Allocate(freeMap, 0);

            if (success) {
                // Escribir el freeMap a disco ANTES de soltar el lock
                freeMap->WriteBack(freeMapFile);
                freeMapLock->Release();

                DEBUG('f', "File %s created, sector %d\n", fileName, sector);
                h->WriteBack(sector);
                parentDir->WriteBack(parentDirFile);
            } else {
                DEBUG('f', "File %s cannot be created: no free space for data blocks\n", fileName);
                freeMapLock->Release();
            }

            delete h;
        }
        delete freeMap;
    }
    
    delete parentDirFile;
    delete parentDir;
    delete[] fileName;
    
    return success;
}

/// Open a file for reading and writing.
///
/// To open a file:
/// 1. Find the location of the file's header, using the directory.
/// 2. Bring the header into memory.
///
/// * `name` is the text name of the file to be opened.
OpenFile *
FileSystem::Open(const char *name)
{
    ASSERT(name != nullptr);

    PathResolution resolution;
    bool resolutionSuccess = ResolvePath(name, &resolution);

    if (!resolutionSuccess) {
        DEBUG('f', "File %s cannot be opened: invalid path\n", name);
        return nullptr;
    }

    char *fileName = resolution.fileName;
    int parentDirSector = resolution.parentDirSector;
    int sector;

    if (strcmp(fileName, "") == 0) {
        sector = parentDirSector;
    } else {
        OpenFile *parentDirFile = new OpenFile(new OpenFileEntry(parentDirSector));
        Directory *parentDir = new Directory(NUM_DIR_ENTRIES);
        parentDir->FetchFrom(parentDirFile);

        sector = parentDir->Find(fileName);

        if (sector == -1) {
            DEBUG('f', "File %s cannot be opened: not found\n", name);
            delete[] fileName;
            return nullptr;
        }

        delete parentDir;
        delete parentDirFile;
    }

    DEBUG('f', "Opening file %s\n", name);
    OpenFileEntry *entry = nullptr;

    openFileTableLock->Acquire();
    for (unsigned i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
        if (openFileTable->HasKey(i)) {
            OpenFileEntry *e = openFileTable->Get(i);
            if (e->GetSector() == sector) {
                entry = e;
                break;
            }
        }
    }

    if (entry == nullptr) {
        entry = new OpenFileEntry(sector);
        if (openFileTable->Add(entry) == -1) {
            DEBUG('f', "Open file table is full. Cannot open file %s\n", name);
            delete entry;
            openFileTableLock->Release();
            delete[] fileName;
            return nullptr;
        }
    }

    entry->AcquireMetaLock();
    entry->AddOpener();
    entry->ReleaseMetaLock();
    openFileTableLock->Release();

    OpenFile *openFile = new OpenFile(entry);  // `fileName` was found in directory (or it's root).

    delete[] fileName;
    return openFile;
}

/// Delete a file from the file system.
///
/// This requires:
/// 1. Remove it from the directory.
/// 2. Delete the space for its header.
/// 3. Delete the space for its data blocks.
/// 4. Write changes to directory, bitmap back to disk.
///
/// Return true if the file was deleted, false if the file was not in the
/// file system.
///
/// * `name` is the text name of the file to be removed.
bool
FileSystem::Remove(const char *name)
{
    ASSERT(name != nullptr);

    PathResolution resolution;
    bool resolutionSuccess = ResolvePath(name, &resolution);

    if (!resolutionSuccess) {
        DEBUG('f', "File %s cannot be removed: invalid path\n", name);
        return false;
    }

    if (strcmp(resolution.fileName, "") == 0) {
        DEBUG('f', "File %s cannot be removed: invalid name\n", name);
        delete[] resolution.fileName;
        return false;
    }

    char *fileName = resolution.fileName;
    int parentDirSector = resolution.parentDirSector;

    OpenFile *parentDirFile = new OpenFile(new OpenFileEntry(parentDirSector));
    Directory *parentDir = new Directory(NUM_DIR_ENTRIES);
    parentDir->FetchFrom(parentDirFile);

    int sector = parentDir->Find(fileName);
    if (sector == -1) {
        DEBUG('f', "File %s cannot be removed: file not found\n", fileName);
        delete parentDir;
        delete parentDirFile;
        delete[] fileName;
        return false;  // file not found
    }

    FileHeader *fileH = new FileHeader;
    fileH->FetchFrom(sector);
    if (fileH->IsDirectory()) {
        DEBUG('f', "File %s cannot be removed: is a directory\n", fileName);
        delete fileH;
        delete parentDir;
        delete parentDirFile;
        delete[] fileName;
        return false;
    }

    bool isOpen = false;
    openFileTableLock->Acquire();
    for (unsigned i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
        if (openFileTable->HasKey(i)) {
            OpenFileEntry *e = openFileTable->Get(i);
            if (e->GetSector() == sector) {
                e->AcquireMetaLock();
                e->MarkForRemoval();
                e->ReleaseMetaLock();
                isOpen = true;
                break;
            }
        }
    }
    openFileTableLock->Release();

    parentDir->Remove(fileName);
    parentDir->WriteBack(parentDirFile);    // Flush to disk.

    if (!isOpen) {
        freeMapLock->Acquire();
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);
        freeMap->FetchFrom(freeMapFile);

        fileH->Deallocate(freeMap);  // Remove data blocks.
        freeMap->Clear(sector);      // Remove header block.
        freeMap->WriteBack(freeMapFile);  // Flush to disk.
        freeMapLock->Release();
        
        delete freeMap;
    }
    
    delete fileH;
    delete parentDir;
    delete parentDirFile;
    delete[] fileName;
    return true;
}

/// List all the files in the file system directory.
void
FileSystem::List()
{
    Directory *dir = new Directory(NUM_DIR_ENTRIES);

    dir->FetchFrom(directoryFile);
    dir->List();

    delete dir;
}

/// Close a file. Remove it from open files table and directory if needed.
///
/// * `entry` points to the open file entry for the file being closed.
void
FileSystem::CloseFile(OpenFileEntry *entry)
{
    ASSERT(entry != nullptr);
    
    openFileTableLock->Acquire();
    entry->AcquireMetaLock();
    entry->RemoveOpener();
    
    bool shouldDelete = (entry->GetOpenerCount() <= 0);
    bool pending = entry->IsPendingRemoval();
    int sector = entry->GetSector();

    if (shouldDelete) {
        for (unsigned i = 0; i < OPEN_FILE_TABLE_SIZE; i++) {
            if (openFileTable->HasKey(i) && openFileTable->Get(i) == entry) {
                openFileTable->Remove(i);
                break;
            }
        }
    }
    entry->ReleaseMetaLock();
    openFileTableLock->Release();

    if (shouldDelete) {
        if (pending) {
            FileHeader *fileH = new FileHeader;
            fileH->FetchFrom(sector);

            freeMapLock->Acquire();
            Bitmap *freeMap = new Bitmap(NUM_SECTORS);
            freeMap->FetchFrom(freeMapFile);

            fileH->Deallocate(freeMap);  // Remove data blocks.
            freeMap->Clear(sector);      // Remove header block.
            freeMap->WriteBack(freeMapFile);  // Flush to disk.
            freeMapLock->Release();

            delete fileH;
            delete freeMap;
        }
        
        delete entry;
    }
}

/// * `sector` is the sector number of the file header.
/// * `hdr` is the file header.
/// * `newSize` is the new size of the file.
bool
FileSystem::ExtendFile(int sector, FileHeader *hdr, unsigned newSize)
{
    DEBUG('f', "Requested extend for file with header at sector %d to %u bytes.\n", sector, newSize);

    freeMapLock->Acquire();

    hdr->FetchFrom(sector);
    if (newSize <= hdr->FileLength()) {
        freeMapLock->Release();
        DEBUG('f', "Sector %d already has length %u >= %u.\n", sector, hdr->FileLength(), newSize);
        return true;
    }

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);

    bool success = hdr->Extend(freeMap, newSize);

    if (success) {
        freeMap->WriteBack(freeMapFile);
        hdr->WriteBack(sector);
    }

    freeMapLock->Release();
    delete freeMap;

    DEBUG('f', "ExtendFile: sector %d %s (length now %u bytes).\n",
          sector, success ? "succeeded" : "failed", hdr->FileLength());

    return success;
}

bool
FileSystem::Mkdir(const char *name) {
    ASSERT(name != nullptr);

    DEBUG('f', "Creating directory %s\n", name);

    PathResolution resolution;
    bool resolutionSuccess = ResolvePath(name, &resolution);

    if (!resolutionSuccess) {
        return false;
    }

    if (strcmp(resolution.fileName, "") == 0) {
        delete[] resolution.fileName;
        return false;
    }

    int parentDirSector = resolution.parentDirSector;
    const char *dirName = resolution.fileName;
    
    OpenFile *parentDirFile = new OpenFile(new OpenFileEntry(parentDirSector));
    Directory *parentDir = new Directory(NUM_DIR_ENTRIES);
    parentDir->FetchFrom(parentDirFile);

    bool success;

    if (parentDir->Find(dirName) != -1) {
        DEBUG('f', "Directory %s already exists\n", dirName);
        success = false;
    } else {
        Bitmap *freeMap = new Bitmap(NUM_SECTORS);

        freeMapLock->Acquire();
        freeMap->FetchFrom(freeMapFile);
        
        int sector = freeMap->Find(); // Buscar sector para el FileHeader
        
        if (sector == -1) {
            DEBUG('f', "Directory %s cannot be created: no free space for directory header\n", dirName);
            success = false;
            freeMapLock->Release();
        } else if (!parentDir->Add(dirName, sector)){
            DEBUG('f', "Directory %s cannot be created: no free space in directory\n", dirName);
            success = false;
            freeMapLock->Release();
        } else {
            FileHeader *h = new FileHeader;
            Directory *newDir = new Directory(NUM_DIR_ENTRIES);

            // Asignar bloques de datos MIENTRAS tenemos el lock del freeMap
            success = h->Allocate(freeMap, DIRECTORY_FILE_SIZE);
            
            if (success) {
                h->SetDirectory(true);
                
                // Guardar los cambios del freeMap en disco ANTES de soltar el lock
                freeMap->WriteBack(freeMapFile);
                freeMapLock->Release();
                
                DEBUG('f', "Directory %s created, sector %d\n", dirName, sector);
                
                OpenFile *dirFile = new OpenFile(new OpenFileEntry(sector));
                
                h->WriteBack(sector);
                
                // Agregar "." y ".." al nuevo directorio
                newDir->Add(".", sector);
                newDir->Add("..", parentDirSector);
                
                newDir->WriteBack(dirFile);
                parentDir->WriteBack(parentDirFile);
                
                delete dirFile; // Evita fugas de memoria del OpenFile
            } else {
                DEBUG('f', "Directory %s cannot be created: no free space for data blocks\n", dirName);
                freeMapLock->Release();
            }

            delete newDir; // Evita fugas de memoria
            delete h;
        }
        delete freeMap;
    }
    
    // Evitar fugas de memoria de las estructuras base
    delete[] resolution.fileName; 
    delete parentDir;
    delete parentDirFile; 
    
    return success;
}

bool
FileSystem::Chdir(const char *name) {
    ASSERT(name != nullptr);

    PathResolution resolution;
    bool success = ResolvePath(name, &resolution);

    if (!success) {
        return false;
    }

    if (strcmp(resolution.fileName, "") == 0) {
        currentThread->SetCwdSector(resolution.parentDirSector);
        delete[] resolution.fileName;
        return true;
    }

    OpenFile *parentDirFile = new OpenFile(new OpenFileEntry(resolution.parentDirSector));
    Directory *parentDir = new Directory(NUM_DIR_ENTRIES);
    parentDir->FetchFrom(parentDirFile);

    int dirSector = parentDir->Find(resolution.fileName);
    if (dirSector == -1) {
        DEBUG('f', "Directory %s cannot be found\n", resolution.fileName);
        delete parentDir;
        delete parentDirFile;
        delete[] resolution.fileName;
        return false;
    }

    FileHeader *dirH = new FileHeader;
    dirH->FetchFrom(dirSector);
    
    if (!dirH->IsDirectory()) {
        DEBUG('f', "File %s is not a directory\n", resolution.fileName);
        delete dirH;
        delete parentDir;
        delete parentDirFile;
        delete[] resolution.fileName;
        return false;
    }

    currentThread->SetCwdSector(dirSector);
    delete[] resolution.fileName;
    delete parentDir;
    delete parentDirFile;
    delete dirH;
    return true;
}

bool
FileSystem::ResolvePath(const char *path, PathResolution* resolution) { // TODO
    if (path == nullptr || resolution == nullptr) {
        return false;
    }

    int currentSector;
    if (path[0] == '/') {
        currentSector = DIRECTORY_SECTOR;
    } else {
        currentSector = currentThread->GetCwdSector();
    }

    char *pathCopy = new char[strlen(path) + 1];
    strcpy(pathCopy, path);

    char *token = strtok(pathCopy, "/");
    char *nextToken = nullptr;

    if (token == nullptr) {
        // La ruta es vacía, "/" o similar ("////")
        delete[] pathCopy;
        resolution->parentDirSector = currentSector;
        char *emptyStr = new char[1];
        emptyStr[0] = '\0';
        resolution->fileName = emptyStr;
        return true;
    }

    while (token != nullptr) {
        nextToken = strtok(nullptr, "/");

        if (nextToken == nullptr) {
            // Último token: es el nombre del archivo o directorio final
            resolution->parentDirSector = currentSector;
            char *finalName = new char[strlen(token) + 1];
            strcpy(finalName, token);
            resolution->fileName = finalName;
            break;
        } else {
            // Token intermedio: buscarlo en el directorio actual
            if (strcmp(token, ".") == 0) {
                // Mantenerse en el mismo directorio
            } else {
                // El caso de ".." debería funcionar automáticamente si 
                // agregaste las entradas "." y ".." al crear el directorio.
                OpenFile *dirFile = new OpenFile(new OpenFileEntry(currentSector));
                Directory *dir = new Directory(NUM_DIR_ENTRIES);
                dir->FetchFrom(dirFile);
                int nextSector = dir->Find(token);
                delete dir;
                delete dirFile;

                if (nextSector == -1) {
                    delete[] pathCopy;
                    return false; // El directorio intermedio no existe
                }

                // Verificar que el sector encontrado sea efectivamente un directorio
                FileHeader *fh = new FileHeader;
                fh->FetchFrom(nextSector);
                bool isDir = fh->IsDirectory();
                delete fh;

                if (!isDir) {
                    delete[] pathCopy;
                    return false; // No se puede atravesar un archivo normal
                }

                currentSector = nextSector;
            }
        }
        token = nextToken;
    }

    delete[] pathCopy;
    return true;
}

static bool
AddToShadowBitmap(unsigned sector, Bitmap *map)
{
    ASSERT(map != nullptr);

    if (map->Test(sector)) {
        DEBUG('f', "Sector %u was already marked.\n", sector);
        return false;
    }
    map->Mark(sector);
    DEBUG('f', "Marked sector %u.\n", sector);
    return true;
}

static bool
CheckForError(bool value, const char *message)
{
    if (!value) {
        DEBUG('f', "Error: %s\n", message);
    }
    return !value;
}

static bool
CheckSector(unsigned sector, Bitmap *shadowMap)
{
    if (CheckForError(sector < NUM_SECTORS,
                      "sector number too big.  Skipping bitmap check.")) {
        return true;
    }
    return CheckForError(AddToShadowBitmap(sector, shadowMap),
                         "sector number already used.");
}

static bool
CheckFileHeader(const RawFileHeader *rh, unsigned num, Bitmap *shadowMap)
{
    ASSERT(rh != nullptr);

    bool error = false;

    DEBUG('f', "Checking file header %u.  File size: %u bytes, number of sectors: %u.\n",
          num, rh->numBytes, rh->numSectors);
    error |= CheckForError(rh->numSectors >= DivRoundUp(rh->numBytes,
                                                        SECTOR_SIZE),
                           "sector count not compatible with file size.");
    error |= CheckForError(rh->numSectors < NUM_DIRECT,
                           "too many blocks.");
    for (unsigned i = 0; i < rh->numSectors; i++) {
        unsigned s = rh->dataSectors[i];
        error |= CheckSector(s, shadowMap);
    }
    return error;
}

static bool
CheckBitmaps(const Bitmap *freeMap, const Bitmap *shadowMap)
{
    bool error = false;
    for (unsigned i = 0; i < NUM_SECTORS; i++) {
        DEBUG('f', "Checking sector %u. Original: %u, shadow: %u.\n",
              i, freeMap->Test(i), shadowMap->Test(i));
        error |= CheckForError(freeMap->Test(i) == shadowMap->Test(i),
                               "inconsistent bitmap.");
    }
    return error;
}

static bool
CheckDirectory(const RawDirectory *rd, Bitmap *shadowMap)
{
    ASSERT(rd != nullptr);
    ASSERT(shadowMap != nullptr);

    bool error = false;
    unsigned nameCount = 0;
    const char **knownNames = new const char*[rd->tableSize];

    for (unsigned i = 0; i < rd->tableSize; i++) {
        DEBUG('f', "Checking direntry: %u.\n", i);
        const DirectoryEntry *e = &rd->table[i];

        if (e->inUse) {
            if (strlen(e->name) > FILE_NAME_MAX_LEN) {
                DEBUG('f', "Filename too long.\n");
                error = true;
            }

            // Check for repeated filenames.
            DEBUG('f', "Checking for repeated names.  Name count: %u.\n",
                  nameCount);
            bool repeated = false;
            for (unsigned j = 0; j < nameCount; j++) {
                DEBUG('f', "Comparing \"%s\" and \"%s\".\n",
                      knownNames[j], e->name);
                if (strcmp(knownNames[j], e->name) == 0) {
                    DEBUG('f', "Repeated filename.\n");
                    repeated = true;
                    error = true;
                }
            }
            if (!repeated) {
                knownNames[nameCount] = e->name;
                DEBUG('f', "Added \"%s\" at %u.\n", e->name, nameCount);
                nameCount++;
            }

            // Check sector.
            error |= CheckSector(e->sector, shadowMap);

            // Check file header.
            FileHeader *h = new FileHeader;
            const RawFileHeader *rh = h->GetRaw();
            h->FetchFrom(e->sector);
            error |= CheckFileHeader(rh, e->sector, shadowMap);
            delete h;
        }
    }
    delete [] knownNames;
    return error;
}

bool
FileSystem::Check()
{
    DEBUG('f', "Performing filesystem check\n");
    bool error = false;

    directoryLock->Acquire();
    freeMapLock->Acquire();

    Bitmap *shadowMap = new Bitmap(NUM_SECTORS);
    shadowMap->Mark(FREE_MAP_SECTOR);
    shadowMap->Mark(DIRECTORY_SECTOR);

    DEBUG('f', "Checking bitmap's file header.\n");

    FileHeader *bitH = new FileHeader;
    const RawFileHeader *bitRH = bitH->GetRaw();
    bitH->FetchFrom(FREE_MAP_SECTOR);
    DEBUG('f', "  File size: %u bytes, expected %u bytes.\n"
               "  Number of sectors: %u, expected %u.\n",
          bitRH->numBytes, FREE_MAP_FILE_SIZE,
          bitRH->numSectors, FREE_MAP_FILE_SIZE / SECTOR_SIZE);
    error |= CheckForError(bitRH->numBytes == FREE_MAP_FILE_SIZE,
                           "bad bitmap header: wrong file size.");
    error |= CheckForError(bitRH->numSectors == FREE_MAP_FILE_SIZE / SECTOR_SIZE,
                           "bad bitmap header: wrong number of sectors.");
    error |= CheckFileHeader(bitRH, FREE_MAP_SECTOR, shadowMap);
    delete bitH;

    DEBUG('f', "Checking directory.\n");

    FileHeader *dirH = new FileHeader;
    const RawFileHeader *dirRH = dirH->GetRaw();
    dirH->FetchFrom(DIRECTORY_SECTOR);
    error |= CheckFileHeader(dirRH, DIRECTORY_SECTOR, shadowMap);
    delete dirH;

    Bitmap *freeMap = new Bitmap(NUM_SECTORS);
    freeMap->FetchFrom(freeMapFile);
    Directory *dir = new Directory(NUM_DIR_ENTRIES);
    const RawDirectory *rdir = dir->GetRaw();
    dir->FetchFrom(directoryFile);
    error |= CheckDirectory(rdir, shadowMap);
    delete dir;

    // The two bitmaps should match.
    DEBUG('f', "Checking bitmap consistency.\n");
    error |= CheckBitmaps(freeMap, shadowMap);
    delete shadowMap;
    delete freeMap;

    freeMapLock->Release();
    directoryLock->Release();

    DEBUG('f', error ? "Filesystem check failed.\n"
                     : "Filesystem check succeeded.\n");

    return !error;
}

/// Print everything about the file system:
/// * the contents of the bitmap;
/// * the contents of the directory;
/// * for each file in the directory:
///   * the contents of the file header;
///   * the data in the file.
void
FileSystem::Print()
{
    FileHeader *bitH    = new FileHeader;
    FileHeader *dirH    = new FileHeader;
    Bitmap     *freeMap = new Bitmap(NUM_SECTORS);
    Directory  *dir     = new Directory(NUM_DIR_ENTRIES);

    directoryLock->Acquire();
    freeMapLock->Acquire();

    printf("--------------------------------\n");
    bitH->FetchFrom(FREE_MAP_SECTOR);
    bitH->Print("Bitmap");

    printf("--------------------------------\n");
    dirH->FetchFrom(DIRECTORY_SECTOR);
    dirH->Print("Directory");

    printf("--------------------------------\n");
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();

    printf("--------------------------------\n");
    dir->FetchFrom(directoryFile);
    dir->Print();
    printf("--------------------------------\n");

    freeMapLock->Release();
    directoryLock->Release();

    delete bitH;
    delete dirH;
    delete freeMap;
    delete dir;
}
