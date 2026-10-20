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

// USB keyboards on the original Xbox. XAPI drives any HID boot-protocol keyboard
// plugged into a controller port (through a port-to-USB adapter) as
// XDEVICE_TYPE_DEBUG_KEYBOARD and turns its reports into a queue of keystrokes:
// a Windows virtual key, the ASCII it types, and key-up / repeat / modifier flags.
// As the XDK's DebugKeyboard sample does, the keyboards are opened with XInputOpen
// and the queue drained each frame. XAPI keeps one queue for all keyboards, so
// SDL sees one keyboard while any is plugged in.

#include "../../events/SDL_keyboard_c.h"
#include "SDL_xboxogvideo.h"

#include <xkbd.h>

static HANDLE keyboards[XBOXOG_PORT_COUNT];
static SDL_KeyboardID keyboard_id;
static bool queue_ready;

// Windows virtual key -> SDL scancode, for the keys XAPI's HID table produces.
static SDL_Scancode XBOXOG_ScancodeFromVirtualKey(BYTE vk)
{
    if (vk >= 'A' && vk <= 'Z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (vk - 'A'));
    }
    if (vk >= '1' && vk <= '9') {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (vk - '1'));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return (SDL_Scancode)(SDL_SCANCODE_F1 + (vk - VK_F1));
    }
    if (vk >= VK_F13 && vk <= VK_F24) {
        return (SDL_Scancode)(SDL_SCANCODE_F13 + (vk - VK_F13));
    }
    if (vk >= VK_NUMPAD1 && vk <= VK_NUMPAD9) {
        return (SDL_Scancode)(SDL_SCANCODE_KP_1 + (vk - VK_NUMPAD1));
    }

    switch (vk) {
    case '0': return SDL_SCANCODE_0;
    case VK_NUMPAD0: return SDL_SCANCODE_KP_0;
    case VK_RETURN: return SDL_SCANCODE_RETURN;
    case VK_ESCAPE: return SDL_SCANCODE_ESCAPE;
    case VK_BACK: return SDL_SCANCODE_BACKSPACE;
    case VK_TAB: return SDL_SCANCODE_TAB;
    case VK_SPACE: return SDL_SCANCODE_SPACE;
    case VK_OEM_MINUS: return SDL_SCANCODE_MINUS;
    case VK_OEM_PLUS: return SDL_SCANCODE_EQUALS;
    case VK_OEM_4: return SDL_SCANCODE_LEFTBRACKET;
    case VK_OEM_6: return SDL_SCANCODE_RIGHTBRACKET;
    case VK_OEM_5: return SDL_SCANCODE_BACKSLASH;
    case VK_OEM_1: return SDL_SCANCODE_SEMICOLON;
    case VK_OEM_7: return SDL_SCANCODE_APOSTROPHE;
    case VK_OEM_3: return SDL_SCANCODE_GRAVE;
    case VK_OEM_COMMA: return SDL_SCANCODE_COMMA;
    case VK_OEM_PERIOD: return SDL_SCANCODE_PERIOD;
    case VK_OEM_2: return SDL_SCANCODE_SLASH;
    case VK_OEM_102: return SDL_SCANCODE_NONUSBACKSLASH;
    case VK_CAPITAL: return SDL_SCANCODE_CAPSLOCK;
    case VK_PRINT:
    case VK_SNAPSHOT: return SDL_SCANCODE_PRINTSCREEN;
    case VK_SCROLL: return SDL_SCANCODE_SCROLLLOCK;
    case VK_PAUSE: return SDL_SCANCODE_PAUSE;
    case VK_INSERT: return SDL_SCANCODE_INSERT;
    case VK_HOME: return SDL_SCANCODE_HOME;
    case VK_PRIOR: return SDL_SCANCODE_PAGEUP;
    case VK_DELETE: return SDL_SCANCODE_DELETE;
    case VK_END: return SDL_SCANCODE_END;
    case VK_NEXT: return SDL_SCANCODE_PAGEDOWN;
    case VK_RIGHT: return SDL_SCANCODE_RIGHT;
    case VK_LEFT: return SDL_SCANCODE_LEFT;
    case VK_DOWN: return SDL_SCANCODE_DOWN;
    case VK_UP: return SDL_SCANCODE_UP;
    case VK_NUMLOCK: return SDL_SCANCODE_NUMLOCKCLEAR;
    case VK_DIVIDE: return SDL_SCANCODE_KP_DIVIDE;
    case VK_MULTIPLY: return SDL_SCANCODE_KP_MULTIPLY;
    case VK_SUBTRACT: return SDL_SCANCODE_KP_MINUS;
    case VK_ADD: return SDL_SCANCODE_KP_PLUS;
    case VK_SEPARATOR: return SDL_SCANCODE_KP_ENTER;
    case VK_DECIMAL: return SDL_SCANCODE_KP_PERIOD;
    case VK_OEM_NEC_EQUAL: return SDL_SCANCODE_KP_EQUALS;
    case VK_APPS: return SDL_SCANCODE_APPLICATION;
    case VK_SLEEP: return SDL_SCANCODE_SLEEP;
    case VK_LCONTROL: return SDL_SCANCODE_LCTRL;
    case VK_LSHIFT: return SDL_SCANCODE_LSHIFT;
    case VK_LMENU: return SDL_SCANCODE_LALT;
    case VK_LWIN: return SDL_SCANCODE_LGUI;
    case VK_RCONTROL: return SDL_SCANCODE_RCTRL;
    case VK_RSHIFT: return SDL_SCANCODE_RSHIFT;
    case VK_RMENU: return SDL_SCANCODE_RALT;
    case VK_RWIN: return SDL_SCANCODE_RGUI;
    case VK_HELP: return SDL_SCANCODE_HELP;
    case VK_SELECT: return SDL_SCANCODE_SELECT;
    case VK_EXECUTE: return SDL_SCANCODE_EXECUTE;
    case VK_CLEAR: return SDL_SCANCODE_CLEAR;
    case VK_VOLUME_MUTE: return SDL_SCANCODE_MUTE;
    case VK_VOLUME_UP: return SDL_SCANCODE_VOLUMEUP;
    case VK_VOLUME_DOWN: return SDL_SCANCODE_VOLUMEDOWN;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static void XBOXOG_UpdateKeyboardPresence(void)
{
    bool any = false;
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (keyboards[port]) {
            any = true;
        }
    }
    if (any && !keyboard_id) {
        keyboard_id = SDL_GetNextObjectID();
        SDL_AddKeyboard(keyboard_id, "USB keyboard");
    } else if (!any && keyboard_id) {
        // Release whatever was held when it went: removing a keyboard does not.
        SDL_ResetKeyboard();
        SDL_RemoveKeyboard(keyboard_id);
        keyboard_id = 0;
    }
}

static void XBOXOG_OpenKeyboard(int port)
{
    // NULL polling parameters select XAPI's keyboard defaults (auto-polled).
    keyboards[port] = XInputOpen(XDEVICE_TYPE_DEBUG_KEYBOARD, (DWORD)port, XDEVICE_NO_SLOT, NULL);
}

static void XBOXOG_CloseKeyboard(int port)
{
    if (keyboards[port]) {
        XInputClose(keyboards[port]);
        keyboards[port] = NULL;
    }
}

void XBOXOG_InitKeyboard(void)
{
    XINPUT_DEBUG_KEYQUEUE_PARAMETERS params;

    XBOXOG_InitDevices();
    SDL_zeroa(keyboards);
    keyboard_id = 0;

    // Key-down, held-key repeats and key-up: SDL needs the releases to keep its
    // keyboard state (SDL_GetKeyboardState) right. The repeat timing is the
    // DebugKeyboard sample's.
    SDL_zero(params);
    params.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT |
                     XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP | XINPUT_DEBUG_KEYQUEUE_FLAG_ONE_QUEUE;
    params.dwQueueSize = 64;
    params.dwRepeatDelay = 500;
    params.dwRepeatInterval = 50;
    queue_ready = XInputDebugInitKeyboardQueue(&params) == ERROR_SUCCESS;
    if (!queue_ready) {
        return;
    }

    // XGetDevices also resets the change tracking that XGetDeviceChanges reports from.
    const DWORD connected = XGetDevices(XDEVICE_TYPE_DEBUG_KEYBOARD);
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (connected & (1u << port)) {
            XBOXOG_OpenKeyboard(port);
        }
    }
    XBOXOG_UpdateKeyboardPresence();
}

void XBOXOG_QuitKeyboard(void)
{
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        XBOXOG_CloseKeyboard(port);
    }
    XBOXOG_UpdateKeyboardPresence();
}

void XBOXOG_PollKeyboard(void)
{
    DWORD insertions = 0, removals = 0;
    XINPUT_DEBUG_KEYSTROKE stroke;

    if (!queue_ready) {
        return;
    }

    if (XGetDeviceChanges(XDEVICE_TYPE_DEBUG_KEYBOARD, &insertions, &removals)) {
        // A keyboard unplugged and plugged back in shows up in both masks.
        for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
            if (removals & (1u << port)) {
                XBOXOG_CloseKeyboard(port);
            }
        }
        for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
            if ((insertions & (1u << port)) && !keyboards[port]) {
                XBOXOG_OpenKeyboard(port);
            }
        }
        XBOXOG_UpdateKeyboardPresence();
    }

    while (XInputDebugGetKeystroke(&stroke) == ERROR_SUCCESS) {
        const bool down = !(stroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP);
        const SDL_Scancode scancode = XBOXOG_ScancodeFromVirtualKey(stroke.VirtualKey);

        if (!keyboard_id) {
            continue; // nothing to report it as (the keyboard went while keys were queued)
        }
        // A repeat arrives as another key-down; SDL marks it as a repeat itself.
        SDL_SendKeyboardKey(0, keyboard_id, stroke.VirtualKey, scancode, down);

        // XAPI has already applied Shift and the locks to the ASCII it reports.
        const unsigned char ascii = (unsigned char)stroke.Ascii;
        if (down && ascii >= ' ' && ascii != 127 && !(stroke.Flags & (XINPUT_DEBUG_KEYSTROKE_FLAG_CTRL | XINPUT_DEBUG_KEYSTROKE_FLAG_ALT))) {
            const char text[2] = { (char)ascii, '\0' };
            SDL_SendKeyboardText(text);
        }
    }
}

#endif // SDL_VIDEO_DRIVER_XBOXOG
