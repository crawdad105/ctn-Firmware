#pragma once

enum DEBUG_THREAD_COMMAND{
    DEBUG_COMMAND_NONE,
    DEBUG_COMMAND_EVENT_CONTINUE,
    DEBUG_COMMAND_EVENT_READ
};

const DebugEventInfo *GetDebugEvents(u32* count);

bool SendDebugThreadCommand(u32 type);
bool SendDebugThreadCommandWait(u32 type, u32 maxWaitMs);
bool StartProcessDebugThread(Process *proc);
bool StopProcessDebugThread(u32 waitTimMs);
bool ShouldProcessDebugThreadBeRunning();
bool IsProcessDebugThreadRunning();
bool IsBlockingDebugEvent();

Window* debugWindowCreate();