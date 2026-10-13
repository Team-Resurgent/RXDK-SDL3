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

#ifndef SDL_xboxogvideo_h_
#define SDL_xboxogvideo_h_

#include "../SDL_sysvideo.h"
#include "../../core/xboxog/SDL_xboxog.h"

// How a display mode is presented (D3DPRESENTFLAG_* and refresh rate).
struct SDL_DisplayModeData
{
    DWORD present_flags;
    UINT refresh_rate;
};

struct SDL_VideoData
{
    Direct3D *d3d;
    D3DDevice *device; // the console has exactly one; owned by the window that created it
    SDL_Window *device_window;
    bool widescreen;   // the dashboard's Widescreen setting
};

struct SDL_WindowData
{
    D3DPRESENT_PARAMETERS present;
};

// Creates the D3D device for the window, or resets it to the window's display mode.
extern bool XBOXOG_ApplyWindowMode(SDL_VideoDevice *_this, SDL_Window *window);

extern bool XBOXOG_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch);
extern bool XBOXOG_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects);
extern void XBOXOG_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window);

#endif // SDL_xboxogvideo_h_
