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

// Semaphores as kernel semaphore objects. The kernel cannot report a
// semaphore's count, so it is mirrored in an atomic for SDL_GetSemaphoreValue().

#include "../../core/xboxog/SDL_xboxog.h"

struct SDL_Semaphore
{
    HANDLE id;
    SDL_AtomicInt count;
};

SDL_Semaphore *SDL_CreateSemaphore(Uint32 initial_value)
{
    SDL_Semaphore *sem = (SDL_Semaphore *)SDL_malloc(sizeof(*sem));
    if (!sem) {
        return NULL;
    }

    sem->id = CreateSemaphore(NULL, (LONG)initial_value, 32 * 1024, NULL);
    if (!sem->id) {
        SDL_free(sem);
        SDL_SetError("Couldn't create semaphore");
        return NULL;
    }
    SDL_SetAtomicInt(&sem->count, (int)initial_value);
    return sem;
}

void SDL_DestroySemaphore(SDL_Semaphore *sem)
{
    if (sem) {
        if (sem->id) {
            CloseHandle(sem->id);
        }
        SDL_free(sem);
    }
}

bool SDL_WaitSemaphoreTimeoutNS(SDL_Semaphore *sem, Sint64 timeoutNS)
{
    DWORD dwMilliseconds;

    if (!sem) {
        return true;
    }

    if (timeoutNS < 0) {
        dwMilliseconds = INFINITE;
    } else {
        dwMilliseconds = (DWORD)SDL_NS_TO_MS(timeoutNS);
    }

    if (WaitForSingleObject(sem->id, dwMilliseconds) == WAIT_OBJECT_0) {
        SDL_AddAtomicInt(&sem->count, -1);
        return true;
    }
    return false;
}

Uint32 SDL_GetSemaphoreValue(SDL_Semaphore *sem)
{
    if (!sem) {
        return 0;
    }
    return (Uint32)SDL_GetAtomicInt(&sem->count);
}

void SDL_SignalSemaphore(SDL_Semaphore *sem)
{
    if (!sem) {
        return;
    }

    // Count before releasing, so a waiter that wakes immediately never sees the count go negative.
    SDL_AddAtomicInt(&sem->count, 1);
    if (!ReleaseSemaphore(sem->id, 1, NULL)) {
        SDL_AddAtomicInt(&sem->count, -1);
    }
}

#endif // SDL_THREAD_XBOXOG
