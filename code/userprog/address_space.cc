/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"

#include <string.h>
#include <stdio.h>


/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    ASSERT(executable_file != nullptr);

    Executable exe (executable_file);
    ASSERT(exe.CheckMagic());

    // How big is address space?

    unsigned size = exe.GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    size = numPages * PAGE_SIZE;

#ifndef DEMAND_LOADING
    if (numPages > physicalMemoryMap->CountClear()) {
        DEBUG('a', "Error: Not enough physical memory to load program (need %u pages, have %u clear).\n",
              numPages, physicalMemoryMap->CountClear());
        pageTable = nullptr;
        return;
    }
#endif

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

    pageTable = new TranslationEntry[numPages];
#ifdef USE_SWAP
    inSwap = new bool[numPages];
#endif
    ownerPid = currentThread->pid;

    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        
#ifdef DEMAND_LOADING
        pageTable[i].physicalPage = 0;
        pageTable[i].valid        = false;
#else
#ifdef USE_SWAP
        int free = physicalMemoryMap->Find(i, this);
#else
        int free = physicalMemoryMap->Find();
#endif
        if (free == -1) {
            DEBUG('a', "Error: Not enough physical memory to load program.\n"); // TODO if swap
            for (unsigned j = 0; j < i; j++) {
                physicalMemoryMap->Clear(pageTable[j].physicalPage);
            }
            delete [] pageTable;
            pageTable = nullptr;
#ifdef USE_SWAP
            delete [] inSwap;
            inSwap = nullptr;
#endif
            return;
        }

        pageTable[i].physicalPage = free;
        pageTable[i].valid        = true;
#endif
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
#ifdef USE_SWAP
        inSwap[i] = false;
#endif
    }

#ifdef DEMAND_LOADING
    executableFile = executable_file;
#else
    char *mainMemory = machine->mainMemory;
    for (unsigned i = 0; i < numPages; i++) {
        uint32_t pAddr = pageTable[i].physicalPage * PAGE_SIZE;
        memset(&mainMemory[pAddr], 0, PAGE_SIZE);
    }

    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    if (codeSize > 0) {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n",
            virtualAddr, codeSize);

        for (uint32_t i = 0; i < codeSize; i++) {
            uint32_t vAddr = virtualAddr + i;
            uint32_t vPage = vAddr / PAGE_SIZE;
            uint32_t offset = vAddr % PAGE_SIZE;
            uint32_t pAddr = (pageTable[vPage].physicalPage * PAGE_SIZE) + offset;
            exe.ReadCodeBlock(&mainMemory[pAddr], 1, i);
        }
    }
    if (initDataSize > 0) {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n",
            virtualAddr, initDataSize);
        for (uint32_t i = 0; i < initDataSize; i++) {
            uint32_t vAddr = virtualAddr + i;
            uint32_t vPage = vAddr / PAGE_SIZE;
            uint32_t offset = vAddr % PAGE_SIZE;
            uint32_t pAddr = (pageTable[vPage].physicalPage * PAGE_SIZE) + offset;
            exe.ReadDataBlock(&mainMemory[pAddr], 1, i);
        }
    }
#endif

#ifdef USE_SWAP
    char swapFileName[32];
    snprintf(swapFileName, sizeof(swapFileName), "SWAP.%d", ownerPid);
    fileSystem->Create(swapFileName, numPages * PAGE_SIZE);
    swapFile = fileSystem->Open(swapFileName);
#endif
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    if (pageTable != nullptr) {
        for (unsigned int i = 0; i < numPages; i++) {
            if (pageTable[i].valid) {
                physicalMemoryMap->Clear(pageTable[i].physicalPage);
            }
        }
        delete [] pageTable;
    }
#ifdef DEMAND_LOADING
    if (executableFile != nullptr) {
        delete executableFile;
    }
#endif
#ifdef USE_SWAP
    delete [] inSwap;
    if (swapFile != nullptr) {
        delete swapFile;
    }

    char swapFileName[32];
    snprintf(swapFileName, sizeof(swapFileName), "SWAP.%d", ownerPid);
    fileSystem->Remove(swapFileName);
#endif
}

bool
AddressSpace::IsValid() const
{
    return pageTable != nullptr;
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
#ifdef USE_TLB
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        TranslationEntry tlbPage = tlb[i];
        if (tlbPage.valid) {
            pageTable[tlbPage.virtualPage].use   = tlbPage.use;
            pageTable[tlbPage.virtualPage].dirty = tlbPage.dirty;
        }
    }
#endif
}

/// On a context switch, restore the machine state so that this address space
/// can run.
void
AddressSpace::RestoreState()
{
#ifdef USE_TLB
DEBUG('v', "Clearing TLB for new process\n");
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for (unsigned i = 0; i < TLB_SIZE; i++) {
        tlb[i].valid = false;
    }
#else
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
}

/// Load a page into memory
bool
AddressSpace::LoadPage(unsigned vpn) {
    if (vpn >= numPages) {
        return false;
    }

    bool success = false;
    if (pageTable[vpn].valid) {
        success = true;
    }

#ifdef USE_SWAP
    if (!success && inSwap[vpn]) {
        success = SwapIn(vpn);
    }
#endif

#ifdef DEMAND_LOADING
    if (!success) {
        success = LoadFromExecutable(vpn);
    }
#endif

#ifdef USE_TLB
    if (success) {
        LoadIntoTLB(vpn);
    }
#endif

    return success;
}

#ifdef DEMAND_LOADING
/// Loads a page from executable file into memory
bool
AddressSpace::LoadFromExecutable(unsigned vpn) {
#ifdef USE_SWAP
    int freeFrame = physicalMemoryMap->Find(vpn, this);
    if (freeFrame == -1) {
        freeFrame = physicalMemoryMap->PickVictim();
        AddressSpace *victimSpace = physicalMemoryMap->GetSpace(freeFrame);
        unsigned victimVpn = physicalMemoryMap->GetVirtualPage(freeFrame);
        victimSpace->SwapOut(victimVpn);
        physicalMemoryMap->Mark(freeFrame, vpn, this);
    }
#else
    int freeFrame = physicalMemoryMap->Find();
    if (freeFrame == -1) {
        return false;
    }
#endif

    char *mainMemory = machine->mainMemory;
    uint32_t pAddr = freeFrame * PAGE_SIZE;

    memset(&mainMemory[pAddr], 0, PAGE_SIZE);

    Executable exe(executableFile);
    ASSERT(exe.CheckMagic());

    uint32_t pageStart = vpn * PAGE_SIZE;
    uint32_t pageEnd = pageStart + PAGE_SIZE;

    uint32_t codeStart = exe.GetCodeAddr();
    uint32_t codeEnd = codeStart + exe.GetCodeSize();
    uint32_t overlapCodeStart = (pageStart > codeStart) ? pageStart : codeStart;
    uint32_t overlapCodeEnd = (pageEnd < codeEnd) ? pageEnd : codeEnd;

    if (overlapCodeStart < overlapCodeEnd) {
        uint32_t size = overlapCodeEnd - overlapCodeStart;
        uint32_t offsetInPage = overlapCodeStart - pageStart;
        uint32_t offsetInSegment = overlapCodeStart - codeStart;
        exe.ReadCodeBlock(&mainMemory[pAddr + offsetInPage], size, offsetInSegment);
    }

    uint32_t dataStart = exe.GetInitDataAddr();
    uint32_t dataEnd = dataStart + exe.GetInitDataSize();
    uint32_t overlapDataStart = (pageStart > dataStart) ? pageStart : dataStart;
    uint32_t overlapDataEnd = (pageEnd < dataEnd) ? pageEnd : dataEnd;

    if (overlapDataStart < overlapDataEnd) {
        uint32_t size = overlapDataEnd - overlapDataStart;
        uint32_t offsetInPage = overlapDataStart - pageStart;
        uint32_t offsetInSegment = overlapDataStart - dataStart;
        exe.ReadDataBlock(&mainMemory[pAddr + offsetInPage], size, offsetInSegment);
    }

    pageTable[vpn].physicalPage = freeFrame;
    pageTable[vpn].valid = true;
    pageTable[vpn].dirty = false;
    pageTable[vpn].use = false;
    return true;
}
#endif

#ifdef USE_SWAP
/// Loads a page from swap file into memory
bool
AddressSpace::SwapIn(unsigned vpn) {
    ASSERT(vpn < numPages);
    ASSERT(inSwap[vpn]);

    stats->numSwapIns++;

    int freeFrame = physicalMemoryMap->Find(vpn, this);
    if (freeFrame == -1) {
        freeFrame = physicalMemoryMap->PickVictim();
        AddressSpace *victimSpace = physicalMemoryMap->GetSpace(freeFrame);
        unsigned victimVpn = physicalMemoryMap->GetVirtualPage(freeFrame);
        victimSpace->SwapOut(victimVpn);
        physicalMemoryMap->Mark(freeFrame, vpn, this);
    }

    char *mainMemory = machine->mainMemory;
    uint32_t pAddr = freeFrame * PAGE_SIZE;
    uint32_t swapOffset = vpn * PAGE_SIZE;

    swapFile->ReadAt(&mainMemory[pAddr], PAGE_SIZE, swapOffset);

    pageTable[vpn].physicalPage = freeFrame;
    pageTable[vpn].valid = true;
    pageTable[vpn].dirty = false;
    pageTable[vpn].use = false;

    DEBUG('v', "vpn %u loaded from swap in frame %u\n", vpn, freeFrame);
    return true;
}

/// Swaps out a page from memory to swap file
bool
AddressSpace::SwapOut(unsigned vpn) {
    ASSERT(vpn < numPages);
    ASSERT(pageTable[vpn].valid);

    stats->numSwapOuts++;
    DEBUG('v', "Requested swap out for vpn %u\n", vpn);

#ifdef USE_TLB
    if (currentThread->space == this) {
        TranslationEntry *tlb = machine->GetMMU()->tlb;
        for (unsigned i = 0; i < TLB_SIZE; i++) {
            if (tlb[i].valid && tlb[i].physicalPage == pageTable[vpn].physicalPage) {
                pageTable[vpn].dirty = tlb[i].dirty;
                pageTable[vpn].use   = tlb[i].use;
                tlb[i].valid = false;
                break;
            }
        }
    }
#endif

    unsigned physFrame = pageTable[vpn].physicalPage;
    char *mainMemory = machine->mainMemory;
    uint32_t pAddr = physFrame * PAGE_SIZE;

    if (pageTable[vpn].dirty) {
        uint32_t swapOffset = vpn * PAGE_SIZE;
        swapFile->WriteAt(&mainMemory[pAddr], PAGE_SIZE, swapOffset);
        inSwap[vpn] = true;
    }

    pageTable[vpn].valid = false;
    pageTable[vpn].dirty = false;
    physicalMemoryMap->Clear(physFrame);

    DEBUG('v', "vpn %u swapped out from frame %u\n", vpn, physFrame);
    return true;
}
#endif

#ifdef USE_TLB
void
AddressSpace::LoadIntoTLB(unsigned vpn)
{
    static unsigned tlbVictimIdx = 0;
    TranslationEntry *tlb = machine->GetMMU()->tlb;

    if (tlb[tlbVictimIdx].valid) {
        unsigned victimVpn = tlb[tlbVictimIdx].virtualPage;
        pageTable[victimVpn].use = tlb[tlbVictimIdx].use;
        pageTable[victimVpn].dirty = tlb[tlbVictimIdx].dirty;
    }

    DEBUG('v', "Replacing TLB entry at index %u (circular).\n", tlbVictimIdx);
    tlb[tlbVictimIdx] = pageTable[vpn];
    tlbVictimIdx = (tlbVictimIdx + 1) % TLB_SIZE;
}
#endif

