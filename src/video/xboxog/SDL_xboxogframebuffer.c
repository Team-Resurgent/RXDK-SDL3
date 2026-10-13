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

#ifdef SDL_VIDEO_DRIVER_XBOXOG

#include "../../SDL_properties_c.h"
#include "SDL_xboxogvideo.h"

#define XBOXOG_SURFACE "SDL.internal.window.surface"

bool XBOXOG_CreateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, SDL_PixelFormat *format, void **pixels, int *pitch)
{
    const SDL_PixelFormat surface_format = SDL_PIXELFORMAT_XRGB8888; // the back buffer's format
    SDL_Surface *surface;
    int w, h;

    SDL_GetWindowSizeInPixels(window, &w, &h);
    surface = SDL_CreateSurface(w, h, surface_format);
    if (!surface) {
        return false;
    }

    SDL_SetSurfaceProperty(SDL_GetWindowProperties(window), XBOXOG_SURFACE, surface);
    *format = surface_format;
    *pixels = surface->pixels;
    *pitch = surface->pitch;
    return true;
}

bool XBOXOG_UpdateWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window, const SDL_Rect *rects, int numrects)
{
    SDL_VideoData *data = _this->internal;
    SDL_Surface *surface;
    D3DSurface *backbuffer = NULL;
    D3DLOCKED_RECT locked;
    HRESULT hr;
    int w, h;

    surface = (SDL_Surface *)SDL_GetPointerProperty(SDL_GetWindowProperties(window), XBOXOG_SURFACE, NULL);
    if (!surface) {
        return SDL_SetError("Couldn't find the window framebuffer");
    }
    if (data->device_window != window || !data->device) {
        return SDL_SetError("The window has no Direct3D device");
    }

    hr = IDirect3DDevice8_GetBackBuffer(data->device, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    if (FAILED(hr)) {
        return SDL_SetError("Direct3D GetBackBuffer failed (0x%08lx)", (unsigned long)hr);
    }
    // The back buffer is linear in the console's unified memory, so it can be
    // filled directly; LockRect waits for the GPU to finish with it.
    hr = IDirect3DSurface8_LockRect(backbuffer, &locked, NULL, 0);
    if (FAILED(hr)) {
        IDirect3DSurface8_Release(backbuffer);
        return SDL_SetError("Direct3D LockRect failed (0x%08lx)", (unsigned long)hr);
    }

    // The whole frame is copied: with a discarding swap chain only a full
    // back buffer is defined after Present.
    w = SDL_min(surface->w, (int)window->internal->present.BackBufferWidth);
    h = SDL_min(surface->h, (int)window->internal->present.BackBufferHeight);
    for (int y = 0; y < h; ++y) {
        SDL_memcpy((Uint8 *)locked.pBits + (size_t)y * locked.Pitch,
                   (const Uint8 *)surface->pixels + (size_t)y * surface->pitch, (size_t)w * 4);
    }

    IDirect3DSurface8_UnlockRect(backbuffer);
    IDirect3DSurface8_Release(backbuffer);

    hr = IDirect3DDevice8_Present(data->device, NULL, NULL, NULL, NULL);
    if (FAILED(hr)) {
        return SDL_SetError("Direct3D Present failed (0x%08lx)", (unsigned long)hr);
    }
    return true;
}

void XBOXOG_DestroyWindowFramebuffer(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_ClearProperty(SDL_GetWindowProperties(window), XBOXOG_SURFACE);
}

#endif // SDL_VIDEO_DRIVER_XBOXOG
