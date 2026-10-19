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

// USB mice on the original Xbox. XAPI drives any HID boot-protocol mouse plugged
// into a controller port (through a port-to-USB adapter) as XDEVICE_TYPE_DEBUG_MOUSE.
// XInputGetState hands back the motion of the packets that arrived since the previous
// read, but reading again before another packet arrives returns that same motion: a
// new dwPacketNumber is what says the motion is new, as in the XDK's DebugMouse sample.
// There is no system cursor: SDL keeps the pointer position from the relative motion,
// clamped to the window, and titles draw their own cursor.

#include "../../events/SDL_mouse_c.h"
#include "SDL_xboxogvideo.h"

typedef struct XBOXOG_Mouse
{
    HANDLE device;
    SDL_MouseID id;
    DWORD packet; // dwPacketNumber of the last state used
    BYTE buttons;
} XBOXOG_Mouse;

static XBOXOG_Mouse mice[XBOXOG_PORT_COUNT];
static bool centered;

// XINPUT_DEBUG_MOUSE_* bit -> SDL button.
static const struct
{
    BYTE bit;
    Uint8 button;
} button_map[] = {
    { XINPUT_DEBUG_MOUSE_LEFT_BUTTON, SDL_BUTTON_LEFT },
    { XINPUT_DEBUG_MOUSE_RIGHT_BUTTON, SDL_BUTTON_RIGHT },
    { XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON, SDL_BUTTON_MIDDLE },
    { XINPUT_DEBUG_MOUSE_XBUTTON1, SDL_BUTTON_X1 },
    { XINPUT_DEBUG_MOUSE_XBUTTON2, SDL_BUTTON_X2 },
};

static void XBOXOG_OpenMouse(int port)
{
    XBOXOG_Mouse *m = &mice[port];
    char name[32];

    // NULL polling parameters select auto-polling, so XInputGetState never blocks.
    m->device = XInputOpen(XDEVICE_TYPE_DEBUG_MOUSE, (DWORD)port, XDEVICE_NO_SLOT, NULL);
    if (!m->device) {
        return;
    }
    m->id = SDL_GetNextObjectID();
    m->packet = 0; // nothing has arrived yet
    m->buttons = 0;
    SDL_snprintf(name, sizeof(name), "USB mouse (port %d)", port + 1);
    SDL_AddMouse(m->id, name);
}

static void XBOXOG_CloseMouse(int port)
{
    XBOXOG_Mouse *m = &mice[port];
    SDL_Window *window = SDL_GetMouseFocus();

    if (!m->device) {
        return;
    }
    // Release whatever the mouse was holding before it goes.
    for (int i = 0; i < SDL_arraysize(button_map); ++i) {
        if (m->buttons & button_map[i].bit) {
            SDL_SendMouseButton(0, window, m->id, button_map[i].button, false);
        }
    }
    XInputClose(m->device);
    m->device = NULL;
    SDL_RemoveMouse(m->id);
    m->id = 0;
    m->buttons = 0;
}

void XBOXOG_InitMouse(void)
{
    XBOXOG_InitDevices();

    SDL_zeroa(mice);
    centered = false;
    // XGetDevices also resets the change tracking that XGetDeviceChanges reports from.
    const DWORD connected = XGetDevices(XDEVICE_TYPE_DEBUG_MOUSE);
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (connected & (1u << port)) {
            XBOXOG_OpenMouse(port);
        }
    }
}

void XBOXOG_QuitMouse(void)
{
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        XBOXOG_CloseMouse(port);
    }
}

void XBOXOG_PollMouse(void)
{
    DWORD insertions = 0, removals = 0;
    SDL_Window *window;

    if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_MOUSE, &insertions, &removals)) {
        // A mouse unplugged and plugged back in shows up in both masks.
        for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
            if (removals & (1u << port)) {
                XBOXOG_CloseMouse(port);
            }
        }
        for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
            if ((insertions & (1u << port)) && !mice[port].device) {
                XBOXOG_OpenMouse(port);
            }
        }
    }

    window = SDL_GetMouseFocus();
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        XBOXOG_Mouse *m = &mice[port];
        XINPUT_STATE state;

        if (!m->device) {
            continue;
        }
        // Read even without a window, so motion from before one exists is not saved up.
        SDL_zero(state);
        if (XInputGetState(m->device, &state) != ERROR_SUCCESS || state.dwPacketNumber == m->packet) {
            continue;
        }
        m->packet = state.dwPacketNumber;
        if (!window) {
            continue;
        }

        // Start the pointer in the middle of the screen rather than its top-left corner.
        if (!centered) {
            centered = true;
            SDL_SendMouseMotion(0, window, m->id, false, window->w / 2.0f, window->h / 2.0f);
        }

        const XINPUT_MOUSE *in = &state.DebugMouse;
        if (in->cMickeysX || in->cMickeysY) {
            // HID boot-mouse Y grows downwards, as screen Y does.
            SDL_SendMouseMotion(0, window, m->id, true, (float)in->cMickeysX, (float)in->cMickeysY);
        }
        if (in->cWheel) {
            // Positive is away from the user, as SDL's y.
            SDL_SendMouseWheel(0, window, m->id, 0.0f, (float)in->cWheel, SDL_MOUSEWHEEL_NORMAL);
        }
        if (in->bButtons != m->buttons) {
            for (int i = 0; i < SDL_arraysize(button_map); ++i) {
                const BYTE bit = button_map[i].bit;
                if ((in->bButtons ^ m->buttons) & bit) {
                    SDL_SendMouseButton(0, window, m->id, button_map[i].button, (in->bButtons & bit) != 0);
                }
            }
            m->buttons = in->bButtons;
        }
    }
}

#endif // SDL_VIDEO_DRIVER_XBOXOG
