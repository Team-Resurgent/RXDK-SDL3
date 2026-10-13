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

#include "../SDL_main_callbacks.h"
#include "../../core/xboxog/SDL_xboxog.h"

int SDL_RunApp(int argc, char *argv[], SDL_main_func mainFunction, void *reserved)
{
    int result;

    (void)reserved;

    // The title gets no command line; SDL_CallMainFunction supplies a default argv.
    result = SDL_CallMainFunction(argc, argv, mainFunction);

    // An Xbox title never ends on its own (there is no OS to return to), so
    // finishing the app's main function means going back to the dashboard.
    XLaunchNewImage(NULL, NULL);
    return result;
}

#endif // SDL_PLATFORM_XBOXOG
