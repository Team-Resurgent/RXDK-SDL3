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

#ifdef SDL_PLATFORM_XBOXOG

#include "SDL_xboxog.h"

void XBOXOG_InitDevices(void)
{
    static SDL_InitState init;

    if (SDL_ShouldInit(&init)) {
        XInitDevices(0, NULL);
        SDL_SetInitialized(&init, true);
    }
}

bool XBOXOG_SetErrorFromCode(const char *prefix, DWORD code)
{
    if (prefix && *prefix) {
        return SDL_SetError("%s: error 0x%08lx", prefix, (unsigned long)code);
    }
    return SDL_SetError("error 0x%08lx", (unsigned long)code);
}

bool XBOXOG_SetError(const char *prefix)
{
    return XBOXOG_SetErrorFromCode(prefix, GetLastError());
}

#endif // SDL_PLATFORM_XBOXOG
