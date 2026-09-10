#include <3ds.h>
#include "draw.h" // for drawing things like 
#include "font.h" // for font[]
#include <MyThread.h>
#include "ctn/ctn.h"
#include "ctn/newUI.h"
#include "ctn/memory_.h"
#include "ctn/process.h"
#include "ctn/debug_.h"
#include "ctn/threading.h"

#include <string.h> // memset and strcpy
#include <stdio.h> // for sprintf

// "BKPT #0"
//static const u8 *breakpointBuffer[4] = { 0x70, 0x00, 0x20, 0xE1 };

///
/// Other
///

// apparently 0x38 size, https://www.3dbrew.org/wiki/KEventInfo
#define MAX_DEBUG_EVENTS 64
static DebugEventInfo debugEvents[MAX_DEBUG_EVENTS]; // around 1 page
static u32 debugEventIndex = 0;
static bool debugEventBlocking = false;
static bool debugEventBlockingChange = false;

static void ClearDebugEvent(){
    debugEventIndex = 0; // simulate clear by just telling everything they dont exist
}
static bool AddDebugEvent(DebugEventInfo debugInfo){ // retuns if the event is blocking
    debugEvents[debugEventIndex++] = debugInfo;
    if (debugEventIndex >= MAX_DEBUG_EVENTS){
        debugEventIndex = MAX_DEBUG_EVENTS - 1;
        memmove(&debugEvents[0], &debugEvents[1], sizeof(DebugEventInfo) * (MAX_DEBUG_EVENTS - 1)); // shift elements down to leave room for one more
    }
    return debugInfo.flags & 1 ? true : false;
}

const DebugEventInfo *GetDebugEvents(u32* count){
    if (count != NULL) *count = debugEventIndex;
    return debugEvents;
}

///
/// Thread stuff
///

#define STACK_SIZE PAGE_SIZE
static u8 CTR_ALIGN(8) processDebugThreadStack[STACK_SIZE];
static ThreadData processDebugThread;
static bool processDebugThreadStarted = false;
static bool shouldRunProcessDebugThread = false;
static s32 baseDebugCommandResult = 0;

// retuns if the event should be continued automaticaly
static bool handleDebugThreadEvent(Process *proc, DebugEventInfo debugInfo){
    switch (debugInfo.type) {
    case DBGEVENT_ATTACH_PROCESS: { } break;
    case DBGEVENT_ATTACH_THREAD: {
        proc->Debug.threadCount++;
        if (proc->Debug.threadIndex == 0) proc->Debug.selectedThread = debugInfo.thread_id;
        if (proc->Debug.threadIndex < MAX_DEBUG_THREAD) proc->Debug.threads[proc->Debug.threadIndex++].threadId = debugInfo.thread_id;
        // calculate thread priority
        Handle h = 0;
        if (R_SUCCEEDED(svcOpenThread(&h, proc->handle, debugInfo.thread_id))){
            s32 prior = 65; // lowest priority? (claude says lowest is 63 but GDB uses 65)
            if (R_SUCCEEDED(svcGetThreadPriority(&prior, h))){
                proc->Debug.threads[proc->Debug.threadIndex - 1].threadPriority = (u32)prior;
            } else {
                proc->Debug.threads[proc->Debug.threadIndex - 1].threadPriority = 65;
            }
        }
        svcCloseHandle(h);
    } break;
    case DBGEVENT_EXIT_THREAD: {
        if (proc->Debug.threadCount == 1 && debugInfo.exit_thread.reason == EXITTHREAD_EVENT_EXIT_PROCESS){
            proc->Debug.hardExit = true;
            return false;
        }
        proc->Debug.threadCount--;
    } break;
    case DBGEVENT_EXIT_PROCESS: {
        proc->Debug.hardExit = true;
    } return false;
    case DBGEVENT_EXCEPTION: {

    } return false;
    case DBGEVENT_DLL_LOAD: { } break;
    case DBGEVENT_DLL_UNLOAD: { } break;
    case DBGEVENT_SCHEDULE_IN: { } break;
    case DBGEVENT_SCHEDULE_OUT: { } break;
    case DBGEVENT_SYSCALL_IN: { } break;
    case DBGEVENT_SYSCALL_OUT: { } break;
    case DBGEVENT_OUTPUT_STRING: { } break;
    case DBGEVENT_MAP: { } break;
    }
    return true;
}
// returns the command if success or negative command value if fail, returns 0 if invalid command
static s32 handleDebugThreadCommand(Process *proc, u32 cmd){
    if (!proc->debugging) return 0; // not sure how we got to this point but incase we arn't debugging just return i guess
    switch (cmd) {
    case DEBUG_COMMAND_EVENT_CONTINUE: {
        if (debugEventBlocking == true){
            if (continueDebugEvent(proc->debugHandle)){
                debugEventBlocking = false;
            } else return -DEBUG_COMMAND_EVENT_CONTINUE;
        }
    } return DEBUG_COMMAND_EVENT_CONTINUE;
    case DEBUG_COMMAND_EVENT_READ: {
        if (!debugEventBlocking){
            DebugEventInfo info;
            if (getDebugEvent(proc->debugHandle, &info)){
                // check blocking
                if (info.flags & 1) debugEventBlocking = true;
                else debugEventBlocking = false;
                // add event to buffer
                AddDebugEvent(info);
                // handle event (run continue command if return true)
                if (handleDebugThreadEvent(proc, info))
                    handleDebugThreadCommand(proc, DEBUG_COMMAND_EVENT_CONTINUE); // recursion is a little scary
            } else return -DEBUG_COMMAND_EVENT_READ;
        }
    } return DEBUG_COMMAND_EVENT_READ;
    default: break;
    }
    return 0;
}

static void processDebugThreadLoop(Process *proc){
    // start actual debugger
    if (!StartDebugging(proc)){
        shouldRunProcessDebugThread = false;
        return; // stop if failed, everything should rely on shouldRunProcessDebugThread so we shouldn't have to do anything else
    }
    ClearDebugEvent(); // reset index, could be useful to keep prior logs but oh well
    proc->Debug.threadIndex = 0; // reset thread index
    proc->Debug.hardExit = false;
    proc->Debug.softExit = false;

    while(shouldRunProcessDebugThread){
        if (!debugEventBlocking && (proc->Debug.hardExit || proc->Debug.softExit)) break;

        svcSleepThread(10ULL * TICKS_TO_MS); // 10ms

        bool preIsBlocking = debugEventBlocking;
        // check for debug event
        baseDebugCommandResult = handleDebugThreadCommand(proc, DEBUG_COMMAND_EVENT_READ);
        if (preIsBlocking != debugEventBlocking && debugEventBlocking) debugEventBlockingChange = true;
        if (!debugEventBlocking) debugEventBlockingChange = false; // if not blocking never open the debug menu
        // open the debug menu through rosalina if we break
        if (debugEventBlockingChange && !isctnMenuOpen){ // this is actually not good because it basically disables the rosalina menu by forcing into the proc view
            debugEventBlockingChange = false;
            StartOpenDebuggerMenu = true;
            rosalinaMod_ShouldOpenCustomMenu = true;
            u64 start = svcGetSystemTick();
            while (rosalinaMod_ShouldOpenCustomMenu){ // wait 10ms until menu is open
                svcSleepThread(10ull * TICKS_TO_MS);
                if ((svcGetSystemTick() - start) * TICKS_TO_MS >= 1000ull) break; // quit it taking too long
            }
        }

        ThreadCommand *cmds = processDebugThread.commandBuffer;
        // only handle the first command for now
        ThreadCommand *firstCmd = &cmds[0]; // kind of redundant
        u32 cmd = firstCmd->type;
        if (cmd != DEBUG_COMMAND_NONE){
            s32 res = handleDebugThreadCommand(proc, cmd);
            firstCmd->type = DEBUG_COMMAND_NONE;
            firstCmd->done = true;
            firstCmd->result = res;
        }
    }
    shouldRunProcessDebugThread = false;
    debugEventBlocking = false;
    debugEventBlockingChange = false;
    StartOpenDebuggerMenu = false;
    rosalinaMod_ShouldOpenCustomMenu = false;
    // stop actual debugger
    if (proc->debugging){
        StopDebugging(proc, true);
        //StopProcessDebugThread(false); // we dont need to call stop in this thread, we can just return
    }
}
static void processDebugThreadMain(void *param){
    // start thread stuff before thread starts
    processDebugThreadLoop((Process*)param);
    // stop thread stuff in thread
    processDebugThreadStarted = false;
    ((Process*)param)->threadActive = false;
}

static u32 threadCommandIndex = MAX_THREAD_COMMANDS;
// only one thread should call this at a time
bool SendDebugThreadCommand(u32 type){
    if (threadCommandIndex == MAX_THREAD_COMMANDS) Thread_GetThreadCommandIndex(&processDebugThread, &threadCommandIndex);
    return Thread_SendCommand(&processDebugThread, threadCommandIndex, type);
}
// only one thread should call this at a time
bool SendDebugThreadCommandWait(u32 type, u32 maxWaitMs){
    if (maxWaitMs > 0) {
        if (SendDebugThreadCommand(type)) return true;
        else svcSleepThread(maxWaitMs * TICKS_TO_MS);
    }
    return SendDebugThreadCommand(type);
}
bool StartProcessDebugThread(Process *proc){
    if (processDebugThreadStarted || shouldRunProcessDebugThread) return false;
    proc->threadActive = true;
    processDebugThreadStarted = true;
    shouldRunProcessDebugThread = true;
    if (!Thread_CreateThread(&processDebugThread, processDebugThreadMain, (void*)proc, processDebugThreadStack, STACK_SIZE, 52, 1)){ // 1 = CORE_SYSTEM, same as the rosalina thread
        processDebugThreadStarted = false;
        shouldRunProcessDebugThread = false;
        return false;
    }
    return true;
}
bool StopProcessDebugThread(u32 waitTimeMs){
    shouldRunProcessDebugThread = false;
    if (waitTimeMs > 0) svcSleepThread((waitTimeMs * TICKS_TO_MS));
    return !processDebugThreadStarted;
}
bool ShouldProcessDebugThreadBeRunning(){
    return shouldRunProcessDebugThread;
}
bool IsProcessDebugThreadRunning(){
    return processDebugThreadStarted;
}
bool IsBlockingDebugEvent(){
    return debugEventBlocking;
}


///
/// window stuff
/// 

static void debugWindowUpdate(Window *window){
    window->visible = processDebugThreadStarted;
    if (!processDebugThreadStarted){
        window->active = false;
    }
}
static void debugWindowDraw(Window *window){
    if (!window->minimized){
        s32 top = window->y + 9 + PADDING;
        s32 left = window->x + PADDING;
        UI_DrawStringFormat(left, top, 0x000000, "res:%d", baseDebugCommandResult);
        s32 stringY = top + SPACING_Y;
        for (u32 i = 0; i < MAX_THREAD_COMMANDS; i++) {
            UI_DrawStringFormat(left, stringY, 0x000000, "c%d:%d", i, processDebugThread.commandBuffer[i].result);
            stringY += SPACING_Y;
        }
        
    }
}

//Window *debugWindow;
extern Window* CreateWindow(char* title, u32 x, u32 y, u32 w, u32 h, void (*OnDraw)(Window *window), void (*OnUpdate)(Window *window)); // from proc_view.c
Window* debugWindowCreate(){
    //debugWindow = CreateWindow("Debug", 50, 50, 75, 100, debugWindowDraw, debugWindowUpdate);
    return CreateWindow("Debug", 50, 50, 75, 115, debugWindowDraw, debugWindowUpdate);
}