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

#ifdef SDL_TIMER_XBOXOG

#include "../../core/xboxog/SDL_xboxog.h"

Uint64 SDL_GetPerformanceCounter(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (Uint64)counter.QuadPart;
}

Uint64 SDL_GetPerformanceFrequency(void)
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return (Uint64)frequency.QuadPart;
}

void SDL_SYS_DelayNS(Uint64 ns)
{
    // Sleep() only has millisecond granularity; the kernel takes 100 ns units
    // (negative means relative to now).
    LARGE_INTEGER interval;
    Uint64 ticks = ns / 100;

    if (ticks == 0) {
        SwitchToThread();
        return;
    }
    interval.QuadPart = -(LONGLONG)ticks;
    KeDelayExecutionThread(UserMode, FALSE, &interval);
}

#endif // SDL_TIMER_XBOXOG
