#pragma once

#include <3ds.h> // for things like types

#define PID_NULL 0xFFFFFFFF
#define MAX_PROC_COUNT 64 // this is what rosalina has so i hope its enough

typedef struct ProcessInfo {
    u64 titleId;
    u32 pid;
    char name[9]; // 8 for name, +1 for \0
} ProcessInfo;
typedef struct MemSectionInfo {
    u8 failed;
    u8 readable;
    u32 start;
    u32 size;
    u32 perm;
    u32 state;
} MemSectionInfo;

typedef struct DebugThread{
    u32 threadId;
    u32 threadPriority;
    bool readable;
} DebugThread;

#define MAX_BREAKPOINTS 64
typedef struct Breakpoint{
    u8 originalValue[4];
    u32 address;
} Breakpoint;

#define MAX_SECTIONS 64 + 4
#define MAX_DEBUG_THREAD 127
typedef struct Process {
    union {
        MemSectionInfo sections[MAX_SECTIONS];
        struct{
            MemSectionInfo textSection;
            MemSectionInfo rodataSection;
            MemSectionInfo dataSection;
            MemSectionInfo heapSection;
        };
    };
    Handle handle;
    Handle debugHandle;
    u32 pid;
    char name[9];
    u64 titleId;
    bool locked; // determins if the
    bool active;
    bool threadActive; // used for cross thread safety
    bool debugging;
    struct {
        bool textStartMismatch;
        bool rodataStartMismatch;
        bool dataStartMismatch;
        bool textSizeMismatch;
        bool rodataSizeMismatch;
        bool dataSizeMismatch;
    } MismatchSections;
    struct {
        // code cave cant be at 0
        u32 codeCaveAddr;
        u32 selectedThread;
        u32 threadIndex;
        u32 threadCount;
        DebugThread threads[MAX_DEBUG_THREAD];
        Breakpoint breakpoints[MAX_BREAKPOINTS];
        u32 breakpointCount;
        // true if the program was closed (make sure you do not continue the EXIT event)
        bool hardExit;
        // true if we want to close debugger
        bool softExit;
    } Debug;
} Process;
extern Process targetProcess;

bool GetAMTitleInfo(u64 title, AM_TitleEntry *info, u32 *titleLocation);
bool GetAMTitleUpdateInfo(u64 defaultTitle, AM_TitleEntry *info, u32 *titleLocation);

u32 formatDebugEvent(const DebugEventInfo *eventInfo, char* buffer_64);
bool continueDebugEvent(Handle debugHandle);
bool getDebugEvent(Handle debugHandle, DebugEventInfo *eventInfo);

u32 enumerateProcesses(ProcessInfo *procInfo, u32 maxProcCount);
bool queryProcessMemory(Handle debugHandle, u32 address, MemInfo *result);
void drainDebugEvents(Handle debugHandle);
MemSectionInfo queryMemInfo(Handle normalHandle, u32 address);

void _formatMemoryPermission(char *outbuf, u32 perm);
void _formatUserMemoryState(char *outbuf, u32 state, u32* colour);

void UpdateProcessMemorySections(Process *proc);

bool StartDebugging(Process *proc);
bool StopDebugging(Process *proc, bool shouldDrain);
bool AllocateCodeCave(Process *proc, u32 size, u32 *codeCave);

bool OpenProcess(Process *proc, u32 pid);
bool CloseProcess(Process *proc);