
#include <3ds.h>
#include "ctn/threading.h"

// copied from rosalina but striped away some not needed code and added parameters

static void startThread(void* arg) {
    ThreadData *t = (ThreadData *)arg;
    t->entryPoint(t->param);
    svcExitThread();
}

bool Thread_CreateThread(ThreadData* thread, void (*entrypoint)(void*), void *paramater, u8 *stack, u32 stackSize, int prio, int affinity) {
    thread->stacktop = (u32*)(stack + stackSize);
    thread->param = paramater;
    thread->entryPoint = entrypoint;
    for (u32 i = 0; i < MAX_THREAD_COMMANDS; i++) {
        thread->commandBuffer[i].type = 0;
        thread->commandBuffer[i].result = 0;
        thread->commandBuffer[i].done = true;
    }
    return R_SUCCEEDED(svcCreateThread(&(thread->handle), startThread, (u32)(void*)thread, thread->stacktop, prio, affinity)) ? true : false;
}

// TODO: add thread join maybe

// used to get a command index for calling Thread_SendCommand()
bool Thread_GetThreadCommandIndex(ThreadData* thread, u32 *commandIndex){
    if (commandIndex == NULL) return false; // why even call this if your not going to use it
    if (thread->commandSenderCount == MAX_THREAD_COMMANDS) return false;
    *commandIndex = (thread->commandSenderCount)++;
    return true;
}
// each calling thread should have its own commandIndex so 2 threads cant set the same comand at a time
bool Thread_SendCommand(ThreadData* thread, u32 commandIndex, u32 commandType){
    if (commandIndex >= MAX_THREAD_COMMANDS) return false;
    ThreadCommand *cmd = thread->commandBuffer;
    if (!cmd[commandIndex].done) return false; // command not done, if this was not here it would be overriden which could happen while the command is exacting
    cmd[commandIndex].type = commandType;
    cmd[commandIndex].done = false;
    return true;
}

