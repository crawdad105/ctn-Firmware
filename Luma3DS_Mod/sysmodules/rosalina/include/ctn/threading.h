#pragma once

#include <3ds.h>

#define MAX_THREAD_COMMANDS 8

typedef struct {
    u32 type;
    s32 result;
    bool done;
} ThreadCommand;

// its up to the user to handle commands
typedef struct {
    Handle handle;
    u32* stacktop;
    void (*entryPoint)(void*);
    void *param;
    ThreadCommand commandBuffer[MAX_THREAD_COMMANDS];
    u32 commandSenderCount;
} ThreadData; // cant use ThreadContext thats already a thing

bool Thread_CreateThread(ThreadData* thread, void (*entrypoint)(void*), void *paramater, u8 *stack, u32 stackSize, int prio, int affinity);
bool Thread_GetThreadCommandIndex(ThreadData* thread, u32 *commandIndex);
bool Thread_SendCommand(ThreadData* thread, u32 commandIndex, u32 commandType);