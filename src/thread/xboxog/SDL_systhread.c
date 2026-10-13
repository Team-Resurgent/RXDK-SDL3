/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#include "SDL_internal.h"

#ifdef SDL_THREAD_XBOXOG

// Thread management routines for SDL, over XAPI threads.

#include "../SDL_systhread.h"
#include "../../core/xboxog/SDL_xboxog.h"

// The console has 64 MB of RAM and threads commit their stacks up front, so
// keep the default modest. Callers can still ask for more via SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER.
#define XBOXOG_THREAD_STACK_SIZE_DEFAULT (64 * 1024)

static DWORD WINAPI RunThread(LPVOID data)
{
    SDL_RunThread((SDL_Thread *)data);
    return 0;
}

bool SDL_SYS_CreateThread(SDL_Thread *thread,
                          SDL_FunctionPointer pfnBeginThread,
                          SDL_FunctionPointer pfnEndThread)
{
    DWORD threadid = 0;
    const SIZE_T stacksize = thread->stacksize ? (SIZE_T)thread->stacksize : XBOXOG_THREAD_STACK_SIZE_DEFAULT;

    (void)pfnBeginThread;
    (void)pfnEndThread;

    thread->handle = CreateThread(NULL, stacksize, RunThread, thread, 0, &threadid);
    if (!thread->handle) {
        return SDL_SetError("Not enough resources to create thread");
    }
    thread->threadid = (SDL_ThreadID)threadid;
    return true;
}

void SDL_SYS_SetupThread(const char *name)
{
    (void)name;
}

SDL_ThreadID SDL_GetCurrentThreadID(void)
{
    return (SDL_ThreadID)GetCurrentThreadId();
}

bool SDL_SYS_SetThreadPriority(SDL_ThreadPriority priority)
{
    int value;

    if (priority == SDL_THREAD_PRIORITY_LOW) {
        value = THREAD_PRIORITY_LOWEST;
    } else if (priority == SDL_THREAD_PRIORITY_HIGH) {
        value = THREAD_PRIORITY_HIGHEST;
    } else if (priority == SDL_THREAD_PRIORITY_TIME_CRITICAL) {
        value = THREAD_PRIORITY_TIME_CRITICAL;
    } else {
        value = THREAD_PRIORITY_NORMAL;
    }

    if (!SetThreadPriority(GetCurrentThread(), value)) {
        return XBOXOG_SetError("SetThreadPriority()");
    }
    return true;
}

void SDL_SYS_WaitThread(SDL_Thread *thread)
{
    WaitForSingleObject(thread->handle, INFINITE);
    CloseHandle(thread->handle);
}

void SDL_SYS_DetachThread(SDL_Thread *thread)
{
    CloseHandle(thread->handle);
}

#endif // SDL_THREAD_XBOXOG
