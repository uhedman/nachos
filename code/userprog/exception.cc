/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "machine/synch_console.hh"
#include "args.hh"

#include <stdio.h>


static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

static void
ExecProcess(void* args) {
	currentThread->space->InitRegisters();
	currentThread->space->RestoreState();

	if (args != nullptr) {
		unsigned argc = WriteArgs((char **)args);
		int argv = machine->ReadRegister(STACK_REG);

		machine->WriteRegister(4, argc);
		machine->WriteRegister(5, argv);
		machine->WriteRegister(STACK_REG, argv - 24);
	}

	machine->Run();
	ASSERT(false); // machine->Run() never returns
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_EXIT: {
            int status = machine->ReadRegister(4);
            
            DEBUG('e', "Exit with status %d, initiated by user program.\n", status);
            currentThread->Finish(status);
            break;
        }

        case SC_EXEC: {
            int filenameAddr = machine->ReadRegister(4);
            int joinable = machine->ReadRegister(5);

            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Exec` requested for file `%s`.\n", filename);
            OpenFile *executable = fileSystem->Open(filename);
            if (executable == nullptr) {
                DEBUG('e', "Error: could not open file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }
            
            Thread *newThread = new Thread(filename, bool(joinable), currentThread->GetPriority());

            SpaceId pid = processTable->Add(newThread);
            if (pid == -1) {
                DEBUG('e', "Error: too many processes.\n");
                delete newThread;
                delete executable;
                machine->WriteRegister(2, -1);
                break;
            }
            newThread->pid = pid;
            
            AddressSpace *space = new AddressSpace(executable);
            if (!space->IsValid()) {
                DEBUG('e', "Error: Insufficient memory to load `%s`.\n", filename);
                delete space;
                delete executable;
                processTable->Remove(pid);
                delete newThread;
                machine->WriteRegister(2, -1);
                break;
            }

            newThread->space = space;
#ifndef DEMAND_LOADING
            delete executable;
#endif

            newThread->Fork(ExecProcess, nullptr);
            DEBUG('e', "Thread `%s` created successfully with PID %d.\n", filename, pid);

            machine->WriteRegister(2, pid);
            break;
        }

        case SC_EXEC2: {
            int filenameAddr = machine->ReadRegister(4);
            int joinable = machine->ReadRegister(5);
            int argsAddr = machine->ReadRegister(6);

            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (argsAddr == 0) {
                DEBUG('e', "Error: address to arguments array is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            char **argv = SaveArgs(argsAddr);
            if (argv == nullptr) {
                DEBUG('e', "Error: failed to save arguments for file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Exec2` requested for file `%s`.\n", filename);
            OpenFile *executable = fileSystem->Open(filename);
            if (executable == nullptr) {
                DEBUG('e', "Error: could not open file `%s`.\n", filename);
                delete argv;
                machine->WriteRegister(2, -1);
                break;
            }
            
            Thread *newThread = new Thread(filename, bool(joinable), currentThread->GetPriority());

            SpaceId pid = processTable->Add(newThread);
            if (pid == -1) {
                DEBUG('e', "Error: too many processes.\n");
                delete newThread;
                delete executable;
                delete argv;
                machine->WriteRegister(2, -1);
                break;
            }
            newThread->pid = pid;
            
            AddressSpace *space = new AddressSpace(executable);
            if (!space->IsValid()) {
                DEBUG('e', "Error: Insufficient memory to load `%s`.\n", filename);
                delete space;
                delete executable;
                processTable->Remove(pid);
                delete newThread;
                delete argv;
                machine->WriteRegister(2, -1);
                break;
            }

            newThread->space = space;
#ifndef DEMAND_LOADING
            delete executable;
#endif

            newThread->Fork(ExecProcess, argv);
            DEBUG('e', "Thread `%s` created successfully with PID %d.\n", filename, pid);

            machine->WriteRegister(2, pid);
            break;
        }

        case SC_JOIN: {
            int pid = machine->ReadRegister(4);
            DEBUG('e', "`Join` requested for pid %d.\n", pid);

            if (pid < 0) {
                DEBUG('e', "Error: invalid pid %d.\n", pid);
                machine->WriteRegister(2, -1);
                break;
            }

            Thread *child = processTable->Get(pid);
            if (child == nullptr) {
                DEBUG('e', "Error: invalid pid %d.\n", pid);
                machine->WriteRegister(2, -1);
                break;
            }

            if (child == currentThread) {
                DEBUG('e', "Error: Thread cannot Join itself (PID %d).\n", pid);
                machine->WriteRegister(2, -1);
                break;
            }

            int exitStatus = child->Join();
            DEBUG('e', "Thread `%s` successfully joined process %d. Exit status: %d.\n", 
                  currentThread->GetName(), pid, exitStatus);

            machine->WriteRegister(2, exitStatus);
            break;
        }

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Create` requested for file `%s`.\n", filename);
            if (!fileSystem->Create(filename, 0)) {
                DEBUG('e', "Error: could not create file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File `%s` created successfully.\n", filename);
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_REMOVE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Remove` requested for file `%s`.\n", filename);
            if (!fileSystem->Remove(filename)) {
                DEBUG('e', "Error: could not remove file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File `%s` removed successfully.\n", filename);
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_OPEN: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                DEBUG('e', "Error: address to filename string is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Open` requested for file `%s`.\n", filename);
            OpenFile *file = fileSystem->Open(filename);
            if (file == nullptr) {
                DEBUG('e', "Error: could not open file `%s`.\n", filename);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File `%s` opened successfully.\n", filename);
            int fid = currentThread->AddFile(file);
            if (fid == -1) {
                DEBUG('e', "Error: too many open files.\n");
                delete file;
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "File `%s` assigned id %u.\n", filename, fid);
            machine->WriteRegister(2, fid);
            break;
        }

        case SC_CLOSE: {
            int fid = machine->ReadRegister(4);
            DEBUG('e', "`Close` requested for id %u.\n", fid);

            if (fid < 2) {
                DEBUG('e', "Error: invalid file id %d.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }

            OpenFile *file = currentThread->RemoveFile(fid);
            if (file == nullptr) {
                DEBUG('e', "Error: invalid file id %u.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }

            delete file;

            DEBUG('e', "File id %u closed successfully.\n", fid);
            machine->WriteRegister(2, 0);
            break;
        }

        case SC_READ: {
            int bufferAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int fid = machine->ReadRegister(6);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: address to read buffer is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size < 0) {
                DEBUG('e', "Error: number of bytes to read cannot be negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size == 0) {
                DEBUG('e', "`Read` requested for buffer at %u, size 0, id %u.\n",
                      bufferAddr, fid);
                machine->WriteRegister(2, 0);
                break;
            }

            if (fid < 0) {
                DEBUG('e', "Error: invalid file id %d.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Read` requested for buffer at %u, size %d, id %u.\n",
                  bufferAddr, size, fid);
            int bytesRead = 0;

            if (fid == CONSOLE_INPUT) {
                char *buffer = new char[size];
                for (int i = 0; i < size; i++) {
                    // buffer[i] = synchConsole->GetChar();
                }

                WriteBufferToUser(buffer, bufferAddr, size);
                
                bytesRead = size;
                delete[] buffer;
            } else if (fid == CONSOLE_OUTPUT) {
                DEBUG('e', "Error: cannot read from console output.\n");
                machine->WriteRegister(2, -1);
                break;
            } else {
                OpenFile *file = currentThread->GetFile(fid);
                if (file == nullptr) {
                    DEBUG('e', "Error: invalid file id %u.\n", fid);
                    machine->WriteRegister(2, -1);
                    break;
                }

                char *buffer = new char[size];
                bytesRead = file->Read(buffer, size);

                if (bytesRead < 0) {
                    DEBUG('e', "Error: could not read from file id %u.\n", fid);
                    machine->WriteRegister(2, -1);
                    delete[] buffer;
                    break;
                }
                
                if (bytesRead > 0) {
                    WriteBufferToUser(buffer, bufferAddr, bytesRead);
                }

                delete[] buffer;
            }

            DEBUG('e', "`Read` completed successfully for id %u, bytes read: %d.\n",
                  fid, bytesRead);
            machine->WriteRegister(2, bytesRead);
            break;
        }

        case SC_WRITE: {
            int bufferAddr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int fid = machine->ReadRegister(6);

            if (bufferAddr == 0) {
                DEBUG('e', "Error: address to write buffer is null.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size < 0) {
                DEBUG('e', "Error: number of bytes to write cannot be negative.\n");
                machine->WriteRegister(2, -1);
                break;
            }

            if (size == 0) {
                DEBUG('e', "`Write` requested for buffer at %u, size 0, id %u.\n",
                      bufferAddr, fid);
                machine->WriteRegister(2, 0);
                break;
            }

            if (fid < 0) {
                DEBUG('e', "Error: invalid file id %d.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }

            DEBUG('e', "`Write` requested for buffer at %u, size %d, id %u.\n",
                  bufferAddr, size, fid);
            int bytesWritten = 0;

            if (fid == CONSOLE_INPUT) {
                DEBUG('e', "Error: cannot write to console input.\n");
                machine->WriteRegister(2, -1);
                break;
            } else if (fid == CONSOLE_OUTPUT) {
                char *buffer = new char[size];
                
                ReadBufferFromUser(bufferAddr, buffer, size);

                for (int i = 0; i < size; i++) {
                    // synchConsole->PutChar(buffer[i]);
                }

                bytesWritten = size;
                delete[] buffer;
            } else {
                OpenFile *file = currentThread->GetFile(fid);
                if (file == nullptr) {
                    DEBUG('e', "Error: invalid file id %u.\n", fid);
                    machine->WriteRegister(2, -1);
                    break;
                }
                
                char *buffer = new char[size];
                ReadBufferFromUser(bufferAddr, buffer, size);
                
                bytesWritten = file->Write(buffer, size);
                delete[] buffer;
            }

            DEBUG('e', "`Write` completed successfully for id %u, bytes written: %d.\n",
                  fid, bytesWritten);
            machine->WriteRegister(2, bytesWritten);
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}

static void
PageFaultHandler(ExceptionType _et)
{
    int vaddr = machine->ReadRegister(BAD_VADDR_REG);
    unsigned vpn = (unsigned) vaddr / PAGE_SIZE;

    stats->numPageFaults++;
    DEBUG('e', "PageFault detected. Virtual address: %u, Virtual page number: %u\n", vaddr, vpn);

    if (!currentThread->space->LoadPage(vpn)) {
        DEBUG('e', "Error: could not load page %u.\n", vpn);
        currentThread->Finish(-1);
        return;
    }
}

/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
	machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
