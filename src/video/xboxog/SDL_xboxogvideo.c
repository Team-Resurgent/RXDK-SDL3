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

// Original Xbox video: one display whose modes follow the dashboard's video
// settings, and one full-screen window that owns the console's Direct3D 8
// device. The device is published as a window property for titles that render
// with D3D8 directly; the window framebuffer (SDL's software renderer and
// window surfaces) is copied into its back buffer and presented.

#include "../SDL_pixels_c.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "SDL_xboxogvideo.h"

#define XBOXOGVID_DRIVER_NAME "xboxog"

static void XBOXOG_AddMode(SDL_VideoDisplay *display, int w, int h, UINT hz, DWORD flags)
{
    SDL_DisplayMode mode;
    SDL_DisplayModeData *data = (SDL_DisplayModeData *)SDL_malloc(sizeof(*data));
    if (!data) {
        return;
    }
    data->present_flags = flags;
    data->refresh_rate = hz;

    SDL_zero(mode);
    mode.w = w;
    mode.h = h;
    mode.refresh_rate = (float)hz;
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.internal = data;
    if (!SDL_AddFullscreenDisplayMode(display, &mode)) {
        SDL_free(data);
    }
}

/* The modes the dashboard settings allow ("Dashboard Video Settings" and "Back
   Buffer Resolution and Video Modes" in the XDK documentation):
   - NTSC: 640x480 and 720x480 at 60 Hz, progressive when 480p is enabled.
   - PAL: 640x576 and 720x576 at 50 Hz, plus 640/720x480 at 60 Hz with PAL-60.
   - 720p (1280x720, progressive) and 1080i (1920x1080, interlaced) when
     enabled; both imply widescreen.
   Standard-definition modes carry the widescreen flag when the dashboard is set
   to Widescreen; it changes the aspect ratio, not the back buffer size. */
static bool XBOXOG_GetDisplayModes(SDL_VideoDevice *_this, SDL_VideoDisplay *display)
{
    const DWORD video = XGetVideoFlags();
    const bool pal = XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I;
    const DWORD wide = (video & XC_VIDEO_FLAGS_WIDESCREEN) ? D3DPRESENTFLAG_WIDESCREEN : 0;

    if (video & XC_VIDEO_FLAGS_HDTV_1080i) {
        XBOXOG_AddMode(display, 1920, 1080, 60, D3DPRESENTFLAG_INTERLACED | D3DPRESENTFLAG_WIDESCREEN);
    }
    if (video & XC_VIDEO_FLAGS_HDTV_720p) {
        XBOXOG_AddMode(display, 1280, 720, 60, D3DPRESENTFLAG_PROGRESSIVE | D3DPRESENTFLAG_WIDESCREEN);
    }
    if (pal) {
        XBOXOG_AddMode(display, 720, 576, 50, wide);
        XBOXOG_AddMode(display, 640, 576, 50, wide);
    }
    if (!pal || (video & XC_VIDEO_FLAGS_PAL_60Hz)) {
        const DWORD scan = (video & XC_VIDEO_FLAGS_HDTV_480p) ? D3DPRESENTFLAG_PROGRESSIVE : 0;
        XBOXOG_AddMode(display, 720, 480, 60, scan | wide);
        XBOXOG_AddMode(display, 640, 480, 60, scan | wide);
    }
    return true;
}

static bool XBOXOG_VideoInit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    SDL_VideoDisplay display;
    SDL_DisplayMode mode;
    SDL_DisplayModeData *modedata;
    const DWORD video = XGetVideoFlags();
    const bool pal50 = XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I && !(video & XC_VIDEO_FLAGS_PAL_60Hz);

    data->d3d = Direct3DCreate8(D3D_SDK_VERSION);
    if (!data->d3d) {
        return SDL_SetError("Direct3DCreate8 failed");
    }
    data->widescreen = (video & XC_VIDEO_FLAGS_WIDESCREEN) != 0;

    // 640 wide always works and is scaled to the native mode, so it is the default.
    modedata = (SDL_DisplayModeData *)SDL_malloc(sizeof(*modedata));
    if (!modedata) {
        return false;
    }
    modedata->present_flags = (data->widescreen ? D3DPRESENTFLAG_WIDESCREEN : 0);
    if (!pal50 && (video & XC_VIDEO_FLAGS_HDTV_480p)) {
        modedata->present_flags |= D3DPRESENTFLAG_PROGRESSIVE;
    }
    modedata->refresh_rate = pal50 ? 50 : 60;

    SDL_zero(mode);
    mode.w = 640;
    mode.h = pal50 ? 576 : 480;
    mode.refresh_rate = (float)modedata->refresh_rate;
    mode.format = SDL_PIXELFORMAT_XRGB8888;
    mode.internal = modedata;

    SDL_zero(display);
    display.name = "Xbox";
    display.desktop_mode = mode;
    if (!SDL_AddVideoDisplay(&display, false)) {
        SDL_free(modedata);
        return false;
    }

    XBOXOG_InitMouse();
    return true;
}

static void XBOXOG_VideoQuit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    XBOXOG_QuitMouse();
    if (data->d3d) {
        IDirect3D8_Release(data->d3d);
        data->d3d = NULL;
    }
}

bool XBOXOG_ApplyWindowMode(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata = window->internal;
    SDL_VideoDisplay *display = SDL_GetVideoDisplayForWindow(window);
    const SDL_DisplayMode *mode = (window->current_fullscreen_mode.w && window->current_fullscreen_mode.internal)
                                      ? &window->current_fullscreen_mode
                                      : &display->desktop_mode;
    const SDL_DisplayModeData *modedata = mode->internal;
    D3DPRESENT_PARAMETERS *pp = &wdata->present;
    HRESULT hr;

    SDL_zerop(pp);
    pp->BackBufferWidth = (UINT)mode->w;
    pp->BackBufferHeight = (UINT)mode->h;
    pp->BackBufferFormat = D3DFMT_X8R8G8B8;
    pp->BackBufferCount = 1;
    pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
    // Titles rendering 3D through the published device need depth.
    pp->EnableAutoDepthStencil = TRUE;
    pp->AutoDepthStencilFormat = D3DFMT_D24S8;
    pp->Flags = modedata ? modedata->present_flags : 0;
    pp->FullScreen_RefreshRateInHz = modedata ? modedata->refresh_rate : 60;
    pp->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    if (!data->device) {
        hr = IDirect3D8_CreateDevice(data->d3d, 0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, pp,
                                     &data->device);
        if (FAILED(hr)) {
            data->device = NULL;
            return SDL_SetError("Direct3D CreateDevice %dx%d failed (0x%08lx)", mode->w, mode->h, (unsigned long)hr);
        }
        data->device_window = window;
    } else {
        hr = IDirect3DDevice8_Reset(data->device, pp);
        if (FAILED(hr)) {
            return SDL_SetError("Direct3D Reset %dx%d failed (0x%08lx)", mode->w, mode->h, (unsigned long)hr);
        }
    }

    SDL_SetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_XBOXOG_D3DDEVICE_POINTER, data->device);
    return true;
}

static bool XBOXOG_SetDisplayMode(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    SDL_VideoData *data = _this->internal;

    // The mode becomes the window's on its next ApplyWindowMode; nothing is
    // shown until a window owns the device.
    if (data->device_window && SDL_GetVideoDisplayForWindow(data->device_window) == display) {
        SDL_Window *window = data->device_window;
        const SDL_DisplayModeData *modedata = mode->internal;
        D3DPRESENT_PARAMETERS pp = window->internal->present;
        HRESULT hr;

        pp.BackBufferWidth = (UINT)mode->w;
        pp.BackBufferHeight = (UINT)mode->h;
        pp.Flags = modedata ? modedata->present_flags : 0;
        pp.FullScreen_RefreshRateInHz = modedata ? modedata->refresh_rate : 60;
        hr = IDirect3DDevice8_Reset(data->device, &pp);
        if (FAILED(hr)) {
            return SDL_SetError("Direct3D Reset %dx%d failed (0x%08lx)", mode->w, mode->h, (unsigned long)hr);
        }
        window->internal->present = pp;
    }
    return true;
}

static bool XBOXOG_GetDisplayBounds(SDL_VideoDevice *_this, SDL_VideoDisplay *display, SDL_Rect *rect)
{
    rect->x = 0;
    rect->y = 0;
    rect->w = display->current_mode->w;
    rect->h = display->current_mode->h;
    return true;
}

static bool XBOXOG_CreateWindow(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID create_props)
{
    SDL_VideoData *data = _this->internal;
    SDL_WindowData *wdata;

    // There is one screen and one D3D device.
    if (data->device_window) {
        return SDL_SetError("The Xbox supports only one window");
    }

    wdata = (SDL_WindowData *)SDL_calloc(1, sizeof(*wdata));
    if (!wdata) {
        return false;
    }
    window->internal = wdata;

    if (!XBOXOG_ApplyWindowMode(_this, window)) {
        SDL_free(wdata);
        window->internal = NULL;
        return false;
    }

    SDL_SetMouseFocus(window);
    SDL_SetKeyboardFocus(window);
    return true;
}

static void XBOXOG_DestroyWindow(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *data = _this->internal;

    if (data->device_window == window) {
        SDL_ClearProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_XBOXOG_D3DDEVICE_POINTER);
        IDirect3DDevice8_Release(data->device);
        data->device = NULL;
        data->device_window = NULL;
    }
    SDL_free(window->internal);
    window->internal = NULL;
}

static SDL_FullscreenResult XBOXOG_SetWindowFullscreen(SDL_VideoDevice *_this, SDL_Window *window, SDL_VideoDisplay *display, SDL_FullscreenOp fullscreen)
{
    // Always full screen; apply the mode SDL picked for the window.
    if (window->internal && !XBOXOG_ApplyWindowMode(_this, window)) {
        return SDL_FULLSCREEN_FAILED;
    }
    return SDL_FULLSCREEN_SUCCEEDED;
}

static void XBOXOG_PumpEvents(SDL_VideoDevice *_this)
{
    // Controllers are polled by the joystick subsystem; there are no window events.
    XBOXOG_PollMouse();
}

static void XBOXOG_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device->internal);
    SDL_free(device);
}

static SDL_VideoDevice *XBOXOG_CreateDevice(void)
{
    SDL_VideoDevice *device = (SDL_VideoDevice *)SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        return NULL;
    }
    device->internal = (SDL_VideoData *)SDL_calloc(1, sizeof(SDL_VideoData));
    if (!device->internal) {
        SDL_free(device);
        return NULL;
    }

    device->VideoInit = XBOXOG_VideoInit;
    device->VideoQuit = XBOXOG_VideoQuit;
    device->GetDisplayModes = XBOXOG_GetDisplayModes;
    device->SetDisplayMode = XBOXOG_SetDisplayMode;
    device->GetDisplayBounds = XBOXOG_GetDisplayBounds;
    device->CreateSDLWindow = XBOXOG_CreateWindow;
    device->DestroyWindow = XBOXOG_DestroyWindow;
    device->SetWindowFullscreen = XBOXOG_SetWindowFullscreen;
    device->PumpEvents = XBOXOG_PumpEvents;
    device->CreateWindowFramebuffer = XBOXOG_CreateWindowFramebuffer;
    device->UpdateWindowFramebuffer = XBOXOG_UpdateWindowFramebuffer;
    device->DestroyWindowFramebuffer = XBOXOG_DestroyWindowFramebuffer;
    device->free = XBOXOG_DeleteDevice;
    device->device_caps = VIDEO_DEVICE_CAPS_FULLSCREEN_ONLY;
    return device;
}

// There is no system UI to show a message box with; write it to the debug
// output (xbWatson / the devkit's debugger, or serial on an emulator) and take
// the default button.
static bool XBOXOG_ShowMessageBox(const SDL_MessageBoxData *messageboxdata, int *buttonID)
{
    OutputDebugStringA(messageboxdata->title ? messageboxdata->title : "SDL message box");
    OutputDebugStringA(": ");
    OutputDebugStringA(messageboxdata->message ? messageboxdata->message : "");
    OutputDebugStringA("\n");

    *buttonID = -1;
    for (int i = 0; i < messageboxdata->numbuttons; ++i) {
        if (messageboxdata->buttons[i].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT) {
            *buttonID = messageboxdata->buttons[i].buttonID;
            break;
        }
    }
    if (*buttonID == -1 && messageboxdata->numbuttons > 0) {
        *buttonID = messageboxdata->buttons[0].buttonID;
    }
    return true;
}

VideoBootStrap XBOXOG_bootstrap = {
    XBOXOGVID_DRIVER_NAME, "Xbox Direct3D 8 video driver",
    XBOXOG_CreateDevice,
    XBOXOG_ShowMessageBox,
    false
};

#endif // SDL_VIDEO_DRIVER_XBOXOG
