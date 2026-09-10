#include <3ds.h> // for things like types
#include <csvc.h>
#include <ctn/newUI.h>
#include "ctn/process.h"
#include "ctn/memory_.h"

#include <string.h> // memset and strcpy
#include <stdio.h> // for sprintf

Process targetProcess = { 0 };
bool Proc_IsProcessAttached() { return targetProcess.active; }
bool Proc_IsProcessDebugger() { return targetProcess.debugging; }
Process *Proc_GetProcess() { return &targetProcess; }

bool ReadMemoryToBuffer(Process* proc, u32 address, char* buffer){
    if (!(proc->active)) return false;
    if (!(proc->debugging)) return false;
    if (R_FAILED(svcReadProcessMemory(buffer, proc->debugHandle, PAGE_ROUND(address), PAGE_SIZE))) return false;
    return true;
}

bool GetAMTitleInfo(u64 title, AM_TitleEntry *info, u32 *titleLocation){
    if (R_SUCCEEDED(amInit())){
        u64 titles[1];
        titles[0] = title;
        info[0].titleID = 0;
        bool res = true;
        (*titleLocation) = 0;
        if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_SD, 1, titles, info))){
            (*titleLocation) = 1;
            if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_GAME_CARD, 1, titles, info))){
                (*titleLocation) = 2;
                if (R_FAILED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, titles, info))){ // sketchy nand read
                    res = false;
                }
            }
        }
        amExit();
        return res;
    }
    return false;
}
bool GetAMTitleUpdateInfo(u64 defaultTitle, AM_TitleEntry *info, u32 *titleLocation){
    return GetAMTitleInfo((defaultTitle & 0x00000000FFFFFFFF) | 0x0004000E00000000, info, titleLocation);
}

// https://www.3dbrew.org/wiki/SVC#struct_DebugEventInfo
// use a 64 element buffer, theoretical less may work but just to be safe use 64
u32 formatDebugEvent(const DebugEventInfo *eventInfo, char* buffer_64){
    u32 line = 1;
    char nameBuf[9] = { 0 };
    switch (eventInfo->type) {
    case DBGEVENT_ATTACH_PROCESS: {
        memcpy(nameBuf, eventInfo->attach_process.process_name, 8);
        sprintf(buffer_64, "Attach proc %s (%ld)", nameBuf, eventInfo->attach_process.process_id);
    } break;
    case DBGEVENT_ATTACH_THREAD: {
        sprintf(buffer_64, "Attach thread %ld", eventInfo->attach_thread.creator_thread_id);
    } break;
    case DBGEVENT_EXIT_THREAD: {
        switch (eventInfo->exit_thread.reason) {
            case EXITTHREAD_EVENT_EXIT: { 
                sprintf(buffer_64, "Exit thread \"Exit\"");
            } break;
            case EXITTHREAD_EVENT_TERMINATE: { 
                sprintf(buffer_64, "Exit thread \"Terminate\"");
            } break;
            case EXITTHREAD_EVENT_EXIT_PROCESS: { 
                sprintf(buffer_64, "Exit thread \"Exit process\"");
            } break;
            case EXITTHREAD_EVENT_TERMINATE_PROCESS: { 
                sprintf(buffer_64, "Exit thread \"Terminate process\"");
            } break;
            default: { 
                sprintf(buffer_64, "Exit thread \"Unknown\" (%d)", eventInfo->exit_thread.reason);
            } break;
        }
    } break;
    case DBGEVENT_EXIT_PROCESS: {
        switch (eventInfo->exit_thread.reason) {
            case EXITTHREAD_EVENT_EXIT: { 
                sprintf(buffer_64, "Exit process \"Exit\"");
            } break;
            case EXITTHREAD_EVENT_TERMINATE: { 
                sprintf(buffer_64, "Exit process \"Terminate\"");
            } break;
            case EXITTHREAD_EVENT_EXIT_PROCESS: { 
                sprintf(buffer_64, "Exit process \"Exit process\"");
            } break;
            case EXITTHREAD_EVENT_TERMINATE_PROCESS: { 
                sprintf(buffer_64, "Exit process \"Terminate process\"");
            } break;
            default: { 
                sprintf(buffer_64, "Exit process \"Unknown\" (%d)", eventInfo->exit_process.reason);
            } break;
        }
    } break;
    case DBGEVENT_EXCEPTION: {
        switch (eventInfo->exception.type) {
            case EXCEVENT_UNDEFINED_INSTRUCTION: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Undefine instruction\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_PREFETCH_ABORT: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Prefetch abort\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_DATA_ABORT: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Data abort\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_UNALIGNED_DATA_ACCESS: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Unaligned data access\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_ATTACH_BREAK: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Attach break\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_STOP_POINT: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Stop point\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_USER_BREAK: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"User break\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_DEBUGGER_BREAK: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Debugger break\"", eventInfo->exception.address);
                line = 2;
            } break;
            case EXCEVENT_UNDEFINED_SYSCALL: {
                sprintf(buffer_64, "Exception at 0x%08lX\n \"Undefined sysall\"", eventInfo->exception.address);
                line = 2;
            } break;
            default: {
                sprintf(buffer_64, "Exception at 0x%08lX (%d)", eventInfo->exception.address, eventInfo->exception.type);
            } break;            
        }
    } break;
    case DBGEVENT_DLL_LOAD: {
        sprintf(buffer_64, "Dll Load");
    } break;
    case DBGEVENT_DLL_UNLOAD: {
        sprintf(buffer_64, "Dll Unload");
    } break;
    case DBGEVENT_SCHEDULE_IN: {
        sprintf(buffer_64, "Schedule IN (%llu)", eventInfo->scheduler.clock_tick);
    } break;
    case DBGEVENT_SCHEDULE_OUT: {
        sprintf(buffer_64, "Schedule OUT (%llu)", eventInfo->scheduler.clock_tick);
    } break;
    case DBGEVENT_SYSCALL_IN: {
        sprintf(buffer_64, "Syscall IN %ld (%llu)", eventInfo->syscall.syscall, eventInfo->syscall.clock_tick);
    } break;
    case DBGEVENT_SYSCALL_OUT: {
        sprintf(buffer_64, "Syscall OUT %ld (%llu)", eventInfo->syscall.syscall, eventInfo->syscall.clock_tick);
    } break;
    case DBGEVENT_OUTPUT_STRING: {
        sprintf(buffer_64, "Output string at 0x%08lX (%ld)", eventInfo->output_string.string_addr, eventInfo->output_string.string_addr);
    } break;
    case DBGEVENT_MAP: {
        sprintf(buffer_64, "Map 0x%08lX 0x%08lX", eventInfo->map.mapped_addr, eventInfo->map.mapped_size);
    } break;
    default: {
        sprintf(buffer_64, "Unknown Event");
    } break;
    }
    return line;
}
bool continueDebugEvent(Handle debugHandle){
    DebugFlags flag = 0; // zero doesn't work when dealing with random events like breakpoints or unknown instructions
    flag = DBG_INHIBIT_USER_CPU_EXCEPTION_HANDLERS | DBG_SIGNAL_FAULT_EXCEPTION_EVENTS; // 3
    Result res = svcContinueDebugEvent(debugHandle, flag);
    if (R_FAILED(res)) return false;
    return true;
}
bool getDebugEvent(Handle debugHandle, DebugEventInfo *eventInfo){
    DebugEventInfo _eventInfo;
    Result res = svcGetProcessDebugEvent(&_eventInfo, debugHandle);
    if (R_SUCCEEDED(res)){
        if (eventInfo != NULL) *eventInfo = _eventInfo;
        return true;
    }
    return false;
}
void drainDebugEvents(Handle debugHandle){
    DebugEventInfo eventInfo;
    while (R_SUCCEEDED(svcGetProcessDebugEvent(&eventInfo, debugHandle))){ // flush queue
        //if (eventInfo.type == DBGEVENT_EXIT_PROCESS) return; // immediately stop if reaching exit event, if we flush this the program freezes
        if (eventInfo.flags & 1) continueDebugEvent(debugHandle);
    } // flush events
    while (continueDebugEvent(debugHandle)){ } // flush any other events incase there are any
}

u32 enumerateProcesses(ProcessInfo *procInfo, u32 maxProcCount){
    // copied from whatever Luma3DS did
    s32 procCount = 0;
    u32 procIds[maxProcCount];
    svcGetProcessList(&procCount, procIds, maxProcCount);
    for (int i = 0; i < procCount; i++) {
        int id = procIds[i];
        procInfo[i].pid = id;
        Handle procHandle = 0;
        Result res = svcOpenProcess(&procHandle, id);
        if(R_FAILED(res)){
            procInfo[i].name[0] = 0; // should already be 0
        } else {
            svcGetProcessInfo((s64*)procInfo[i].name, procHandle, 0x10000); // 0x10000 = name
            svcGetProcessInfo((s64*)&(procInfo[i].titleId), procHandle, 0x10001); // 0x10001 = title
        }
        svcCloseHandle(procHandle);
    }
    return procCount;
}
bool queryProcessMemory(Handle debugHandle, u32 address, MemInfo *result){
    PageInfo out; // https://libctru.devkitpro.org/structPageInfo.html
    return R_SUCCEEDED(svcQueryProcessMemory(result, &out, debugHandle, address));
}
MemSectionInfo queryMemInfo(Handle normalHandle, u32 address){ // does not check readability
    MemSectionInfo info;
    MemInfo mem; // https://libctru.devkitpro.org/structMemInfo.html
    PageInfo out; // https://libctru.devkitpro.org/structPageInfo.html
    Result r = svcQueryProcessMemory(&mem, &out, normalHandle, address);
    info.failed = true;
    info.start = 0;
    info.size = 0;
    info.perm = 0;
    info.state = 0;
    if (R_SUCCEEDED(r)){
        info.failed = false;
        info.start = mem.base_addr;
        info.size = mem.size;
        info.perm = (u32)mem.perm;
        info.state = (u32)mem.state;
    }
    return info;
}
bool readableMemory(Handle debugHandle, u32 address){
    if (R_FAILED(svcReadProcessMemory(dataBuffer, debugHandle, address, PAGE_SIZE))) return false;
    else return true;
}

// taken from utils.c
void _formatMemoryPermission(char *outbuf, u32 perm) {
    if (perm == MEMPERM_DONTCARE) {
        strcpy(outbuf, "???");
        return;
    }

    outbuf[0] = perm & MEMPERM_READ ? 'r' : '-';
    outbuf[1] = perm & MEMPERM_WRITE ? 'w' : '-';
    outbuf[2] = perm & MEMPERM_EXECUTE ? 'x' : '-';
    outbuf[3] = '\0';
}
// taken from utils.c
void _formatUserMemoryState(char *outbuf, u32 state, u32* colour) {
    static const char *states[12] = {
        "Free",
        "Reserved",
        "IO",
        "Static",
        "Code",
        "Private",
        "Shared",
        "Continuous",
        "Aliased",
        "Alias",
        "AliasCode",
        "Locked"
    };
    static const int colours[12] = {
        0xFFFFFF,
        0x0100FF,
        0xB200FF,
        0x569cd6,
        0x9B65FF,
        0xFF7466,
        0xD152FF,
        0x8800FF,
        0x8800FF,
        0x8800FF,
        0xAA0000
    };

    if (colour != NULL){
        *colour = state > 11 ? (u32)0xFF0000 : (u32)colours[state];
    }

    strcpy(outbuf, state > 11 ? "Unknown" : states[state]);
}

void UpdateProcessMemorySections(Process *proc){
    u32 procHandle = proc->handle;
    bool debugging = proc->debugging;
    memset(proc->sections, 0, sizeof(proc->sections)); // clear section data

    // get default values (not very usefull)
    MemSectionInfo text = { 0 }, rodata = { 0 }, data = { 0 }, heap = { 0 };
    bool textStartMismatch = false, rodataStartMismatch = false, dataStartMismatch = false;
    bool textSizeMismatch = false, rodataSizeMismatch = false, dataSizeMismatch = false;
    s64 tempAddr;
    MemSectionInfo memInfo = { 0 };
    // not sure what happens if size or location fails to be gotten but is found using queryMemInfo
    // only check for readability if debugging. the known sections do not check for readability the same way as the others
    // we also dont set readability for the known sections if not debugging even if we know its readable as to not be confusing

    // get text section info
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, TEXT_SECTION_ADDR))){
        text.start = (u32)tempAddr;
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, TEXT_SECTION_SIZE))) text.size = (u32)tempAddr;
        else text.failed = true;
        memInfo = queryMemInfo(procHandle, text.start);
        if (!memInfo.failed){
            if (text.start != memInfo.start) textStartMismatch = true;
            if (text.size != memInfo.size) textSizeMismatch = true;
            text.perm = memInfo.perm;
            text.state = memInfo.state;
        }
        text.failed = memInfo.failed;
        if (debugging) text.readable = memInfo.perm & MEMPERM_READ;
    }

    // get read only data section info
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, RODATA_SECTION_ADDR))){
        rodata.start = (u32)tempAddr;
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, RODATA_SECTION_SIZE))) rodata.size = (u32)tempAddr;
        else rodata.failed = true;
        memInfo = queryMemInfo(procHandle, rodata.start);
        if (!memInfo.failed){
            if (rodata.start != memInfo.start) rodataStartMismatch = true;
            if (rodata.size != memInfo.size) rodataSizeMismatch = true;
            rodata.perm = memInfo.perm;
            rodata.state = memInfo.state;
        }
        rodata.failed = memInfo.failed;
        if (debugging) rodata.readable = memInfo.perm & MEMPERM_READ;
    }

    // get data section info
    if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, DATA_SECTION_ADDR))){
        data.start = (u32)tempAddr;
        if (R_SUCCEEDED(svcGetProcessInfo(&tempAddr, procHandle, DATA_SECTION_SIZE))) data.size = (u32)tempAddr;
        else data.failed = true;
        memInfo = queryMemInfo(procHandle, data.start);
        if (!memInfo.failed){
            if (data.start != memInfo.start) dataStartMismatch = true;
            if (data.size != memInfo.size) dataSizeMismatch = true;
            data.perm = memInfo.perm;
            data.state = memInfo.state;
        }
        data.failed = memInfo.failed;
        if (debugging) data.readable = memInfo.perm & MEMPERM_READ;
    }
    
    // get heap section info
    //incase the heap section is not 0x08000000 it will be set so its fine if heap.start != 0x08000000 before (i think)
    heap = queryMemInfo(procHandle, 0x08000000); // this is what rosalina uses for the heap
    if (debugging) heap.readable = heap.perm & MEMPERM_READ;

    proc->MismatchSections.textStartMismatch = textStartMismatch;
    proc->MismatchSections.rodataStartMismatch = rodataStartMismatch;
    proc->MismatchSections.dataStartMismatch = dataStartMismatch;
    proc->MismatchSections.textSizeMismatch = textSizeMismatch;
    proc->MismatchSections.rodataSizeMismatch = rodataSizeMismatch;
    proc->MismatchSections.dataSizeMismatch = dataSizeMismatch;

    proc->textSection = text;
    proc->dataSection = data;
    proc->rodataSection = rodata;
    proc->heapSection = heap;
    
    // read all other sections, it seems that anything past 0x40000000 fails
    MemSectionInfo *info = proc->sections;
    u32 memPos = 0;
    int prevFailed = 0;
    int maxCount = 1024;
    MemInfo mem; // https://libctru.devkitpro.org/structMemInfo.html
    PageInfo out; // https://libctru.devkitpro.org/structPageInfo.html
    for(int i = 4; (i < MAX_SECTIONS && maxCount > 0); i++, maxCount--){
        Result r = svcQueryProcessMemory(&mem, &out, procHandle, memPos);
        if (R_SUCCEEDED(r)){
            if (memPos == mem.base_addr + mem.size) memPos += PAGE_SIZE; // incase mem.size, if 0 go to next page
            else memPos = mem.base_addr + mem.size;
            info[i].failed = false;
            info[i].start = mem.base_addr;
            info[i].size = mem.size;
            info[i].perm = (u32)mem.perm;
            info[i].state = (u32)mem.state;
            prevFailed = false;
            if (debugging){ // check readability of debugging
                info[i].readable = readableMemory(proc->debugHandle, info[i].start); // this wont check the readability of the entire sections so maybe thats something to add later
            }
        } else {
            if (prevFailed){ // group failed, merge into previous
                i--; // decrement i because we want to still be in the same section
                info[i].size += PAGE_SIZE;
            } else {
                info[i].start = memPos;
                info[i].size = PAGE_SIZE;
            }
            info[i].failed = true;
            info[i].perm = 0;
            info[i].state = 0;
            info[i].readable = false;
            memPos += PAGE_SIZE;
            prevFailed = true;
        }
    }
}

bool StartDebugging(Process *proc){
    if (!proc->active) return false;
    if (proc->debugging) return false;
    u32 debugHandle = 0;
    if(R_FAILED(svcDebugActiveProcess(&debugHandle, proc->pid))) return false;
    proc->debugHandle = debugHandle;
    proc->debugging = true;
    UpdateProcessMemorySections(proc); // update memory because we can check for readability
    return true;
}
bool StopDebugging(Process *proc, bool shouldDrain){
    if (!proc->active) return false;
    if (!proc->debugging) return false;
    if (proc->debugHandle != 0){
        if (shouldDrain) drainDebugEvents(proc->debugHandle);
        if(R_FAILED(svcCloseHandle(proc->debugHandle))) return false; // not sure what happens if this fails
        proc->debugging = false;
        proc->debugHandle = 0; // clear handle incase the user tries using it or it gets used by another process
    }
    return true;
}
// draws stuff on fail
bool AllocateCodeCave(Process *proc, u32 size, u32 *codeCave){ // NOTE: this can not be undone, maybe thats something to add but realistically you likely dont want to remove a large chunk of code
    if (!proc->debugging) return false;
    UpdateProcessMemorySections(proc); // update memory so we can check it for a free region
    u32 selfAddr = 0;
    (*codeCave) = 0;

    memRegion mem;
    Result res = mem_alloc(&mem, size, false); // would executable work here? not sure why i did this all if it would
    if (R_SUCCEEDED(res)){     
        selfAddr = mem.address;

        ((u8*)selfAddr)[0] = 1; // test (shows if we can event write to this address)
        
        for (u32 i = 0; i < MAX_SECTIONS; i++) {
            MemSectionInfo info = proc->sections[i];
            if (info.state == MEMSTATE_FREE && info.size >= size) {
                u32 targetAddr = info.start;
                // we cant technically allocate anything (svcControlProcessMemory wont allow MEMOP_ALLOC) so instead we map our memory to the target process'
                // https://www.3dbrew.org/wiki/Memory_layout#NATIVE_FIRM/SAFE_MODE_FIRM_Userland_Memory
                // valid sections i think
                //   0x00100000 to 0x04000000 // code
                //   0x04000000 to 0x08000000 // "Used for mapping buffers during IPC" (probably shouldn't use this but we'il see)
                //   0x08000000 to 0x10000000 // heap
                //   0x10000000 to 0x14000000 // shared memory (we'er making shared memory so i think this is fine)
                //   0x30000000 to 0x38000000 // linear memory (for old 3ds)
                //              to 0x40000000 // for new 3ds
                if ((targetAddr >= 0x00100000 && targetAddr < 0x14000000) || (targetAddr >= 0x30000000 && targetAddr < 0x40000000)){
                    res = svcMapProcessMemoryEx(proc->handle, targetAddr, CUR_PROCESS_HANDLE, selfAddr, size, 0); // sets as shared memory
                    if (R_SUCCEEDED(res)){
                        // make executable now because svcControlMemoryEx wont allow it, also for some reason we cant use pseudo handles so we need to do this for the target process, not ours
                        res = svcControlProcessMemory(proc->handle, targetAddr, targetAddr, size, MEMOP_PROT, MEMPERM_READ | MEMPERM_WRITE | MEMPERM_EXECUTE);   
                        if (R_SUCCEEDED(res)){
                            (*codeCave) = targetAddr;
                            break;
                        } else {
                            UI_DisplayMessageFormat("(%d) Control Fail 0x%08X (0x%08X)", i, targetAddr, res);
                            // unmap if failed
                            res = svcUnmapProcessMemoryEx(proc->handle, targetAddr, size);
                            if (R_FAILED(res)) UI_DisplayMessageFormat("(%d) Unmap Fail 0x%08X (0x%08X)", i, targetAddr, res);
                        }
                    } else UI_DisplayMessageFormat("(%d) Map Fail 0x%08X (0x%08X)", i, targetAddr, res);
                } else {
                    // sketchy memory, so we avoid this
                }
            }
        }
        UpdateProcessMemorySections(proc); // update memory again so we can see the changes

    } else UI_DisplayMessageFormat("Alloc Fail (0x%08X, 0x%08X)", res, selfAddr);

    if (selfAddr == 0) { // could not find a place to map
        mem_free(&mem); // free memory if failed
        return false;
    }

    return true;
}

bool OpenProcess(Process *proc, u32 pid){
    if (proc->active) return false;
    u32 procHandle = 0;
    if(R_FAILED(svcOpenProcess(&procHandle, pid))) return false;
    proc->pid = pid;
    proc->handle = procHandle;
    proc->debugging = false;
    proc->debugHandle = 0; // not sure if this should be the value
    proc->threadActive = false;
    proc->active = true;
    memset(proc->name, 0, 9); // clear name
    // 0x10000 = name, copies 8 bytes
    if(R_FAILED(svcGetProcessInfo((s64*)(proc->name), procHandle, 0x10000))) strcpy(proc->name, "ERROR"); // dont return because its not that bad of an error
    if(R_FAILED(svcGetProcessInfo((s64*)&(proc->titleId), procHandle, 0x10001))) proc->titleId = 0; // dont return because its not that bad of an error
    UpdateProcessMemorySections(proc); // check memory stuff
    return true;
}
bool CloseProcess(Process *proc){
    if (!proc->active) return false;
    if (proc->debugging) StopDebugging(proc, NULL);
    if (proc->handle != 0){
        if (R_FAILED(svcCloseHandle(proc->handle))) return false; // not sure what happens if this fails
        proc->handle = 0; // clear handle incase the user tries using it or it gets used by another process
    }
    proc->active = false;
    return true;
}

bool OpenProcess_Name(const char* name){
    if (targetProcess.active) return false;
    // see enumerateProcesses()

    s32 procCount = 0;
    u32 procIds[64];
    if (R_FAILED(svcGetProcessList(&procCount, procIds, 64))) return false;
    char procName[8];
    Handle procHandle = 0;
    for (int i = 0; i < procCount; i++) {
        if(R_SUCCEEDED(svcOpenProcess(&procHandle, procIds[i]))){
            if (R_SUCCEEDED(svcGetProcessInfo((s64*)procName, procHandle, 0x10000))){ // 0x10000 = name
                if (strncmp(procName, name, 8) == 0){ // compare names
                    OpenProcess(&targetProcess, procIds[i]);
                    svcCloseHandle(procHandle);
                    return true;
                }
            }
        }
        svcCloseHandle(procHandle);
    }
    return false;
}
bool OpenProcess_TitleId(u64 titleId){
    if (targetProcess.active) return false;
    // see enumerateProcesses()
    s32 procCount = 0;
    u32 procIds[64];
    if (R_FAILED(svcGetProcessList(&procCount, procIds, 64))) return false;
    u64 title;
    Handle procHandle = 0;
    for (int i = 0; i < procCount; i++) {
        if(R_SUCCEEDED(svcOpenProcess(&procHandle, procIds[i]))){
            if (R_SUCCEEDED(svcGetProcessInfo((s64*)&title, procHandle, 0x10001))){ // 0x10001 = title id
                if (title == titleId){
                    svcCloseHandle(procHandle);
                    OpenProcess(&targetProcess, procIds[i]);
                    return true;
                }
            }
        }
        svcCloseHandle(procHandle);
    }
    return false;
}