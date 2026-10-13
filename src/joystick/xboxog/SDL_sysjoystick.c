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

#ifdef SDL_JOYSTICK_XBOXOG

// Original Xbox controllers through XInput (XAPI): one gamepad per front port,
// hot-plugged via XGetDeviceChanges, with rumble.

#include "../SDL_sysjoystick.h"
#include "../../core/xboxog/SDL_xboxog.h"

#define XBOXOG_PORT_COUNT 4

// Joystick buttons, in the order the gamepad mapping below refers to them.
enum
{
    XBOXOG_BUTTON_A,
    XBOXOG_BUTTON_B,
    XBOXOG_BUTTON_X,
    XBOXOG_BUTTON_Y,
    XBOXOG_BUTTON_BLACK,
    XBOXOG_BUTTON_WHITE,
    XBOXOG_BUTTON_BACK,
    XBOXOG_BUTTON_START,
    XBOXOG_BUTTON_LEFT_THUMB,
    XBOXOG_BUTTON_RIGHT_THUMB,
    XBOXOG_BUTTON_COUNT
};

// Joystick axes: both sticks, then the two analog triggers.
enum
{
    XBOXOG_AXIS_LEFT_X,
    XBOXOG_AXIS_LEFT_Y,
    XBOXOG_AXIS_RIGHT_X,
    XBOXOG_AXIS_RIGHT_Y,
    XBOXOG_AXIS_LEFT_TRIGGER,
    XBOXOG_AXIS_RIGHT_TRIGGER,
    XBOXOG_AXIS_COUNT
};

typedef struct XBOXOG_Port
{
    HANDLE device;
    SDL_JoystickID instance_id;
    Uint16 vendor;
    Uint16 product;
    Uint16 version;
    // XInputSetState is asynchronous: the feedback block must stay untouched
    // until the kernel reports the request complete.
    XINPUT_FEEDBACK feedback;
    bool rumble_pending;
    Uint16 rumble_low;
    Uint16 rumble_high;
} XBOXOG_Port;

struct joystick_hwdata
{
    int port;
};

static XBOXOG_Port ports[XBOXOG_PORT_COUNT];

static void XBOXOG_OpenPort(int port)
{
    XBOXOG_Port *p = &ports[port];
    XINPUT_DEVICE_DESCRIPTION desc;

    // NULL polling parameters select auto-polling, so XInputGetState never blocks.
    p->device = XInputOpen(XDEVICE_TYPE_GAMEPAD, (DWORD)port, XDEVICE_NO_SLOT, NULL);
    if (!p->device) {
        return;
    }

    SDL_zero(desc);
    if (XInputGetDeviceDescription(p->device, &desc) == ERROR_SUCCESS) {
        p->vendor = desc.wVendorID;
        p->product = desc.wProductID;
        p->version = desc.wVersion;
    } else {
        p->vendor = 0;
        p->product = 0;
        p->version = 0;
    }
    p->rumble_pending = false;
    p->instance_id = SDL_GetNextObjectID();
    SDL_PrivateJoystickAdded(p->instance_id);
}

static void XBOXOG_ClosePort(int port)
{
    XBOXOG_Port *p = &ports[port];

    if (!p->device) {
        return;
    }
    XInputClose(p->device);
    p->device = NULL;
    SDL_PrivateJoystickRemoved(p->instance_id);
    p->instance_id = 0;
}

static int XBOXOG_PortForDeviceIndex(int device_index)
{
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (ports[port].device) {
            if (device_index == 0) {
                return port;
            }
            --device_index;
        }
    }
    return -1;
}

static bool XBOXOG_JoystickInit(void)
{
    XBOXOG_InitDevices();

    SDL_zeroa(ports);
    // XGetDevices also resets the change tracking that XGetDeviceChanges reports from.
    const DWORD connected = XGetDevices(XDEVICE_TYPE_GAMEPAD);
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (connected & (1u << port)) {
            XBOXOG_OpenPort(port);
        }
    }
    return true;
}

static int XBOXOG_JoystickGetCount(void)
{
    int count = 0;
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (ports[port].device) {
            ++count;
        }
    }
    return count;
}

static void XBOXOG_JoystickDetect(void)
{
    DWORD insertions = 0, removals = 0;

    if (!XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals)) {
        return;
    }
    // A controller unplugged and plugged back in shows up in both masks.
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (removals & (1u << port)) {
            XBOXOG_ClosePort(port);
        }
    }
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if ((insertions & (1u << port)) && !ports[port].device) {
            XBOXOG_OpenPort(port);
        }
    }
}

static bool XBOXOG_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    return false;
}

static const char *XBOXOG_JoystickGetDeviceName(int device_index)
{
    return "Xbox Controller";
}

static const char *XBOXOG_JoystickGetDevicePath(int device_index)
{
    return NULL;
}

static int XBOXOG_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

static int XBOXOG_JoystickGetDevicePlayerIndex(int device_index)
{
    // The front port is the player number, as on the console's own menus.
    return XBOXOG_PortForDeviceIndex(device_index);
}

static void XBOXOG_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_GUID XBOXOG_JoystickGetDeviceGUID(int device_index)
{
    const int port = XBOXOG_PortForDeviceIndex(device_index);
    const XBOXOG_Port *p = (port >= 0) ? &ports[port] : NULL;

    return SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, p ? p->vendor : 0, p ? p->product : 0,
                                  p ? p->version : 0, NULL, XBOXOG_JoystickGetDeviceName(device_index), 0, 0);
}

static SDL_JoystickID XBOXOG_JoystickGetDeviceInstanceID(int device_index)
{
    const int port = XBOXOG_PortForDeviceIndex(device_index);
    return (port >= 0) ? ports[port].instance_id : 0;
}

static bool XBOXOG_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    const int port = XBOXOG_PortForDeviceIndex(device_index);
    if (port < 0) {
        return SDL_SetError("No controller at device index %d", device_index);
    }

    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }
    joystick->hwdata->port = port;
    joystick->nbuttons = XBOXOG_BUTTON_COUNT;
    joystick->naxes = XBOXOG_AXIS_COUNT;
    joystick->nhats = 1;

    SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    return true;
}

// Sends the latest requested rumble once the previous request has completed.
// XInputSetState returns ERROR_IO_PENDING and later updates Header.dwStatus;
// a request that fails up front (no rumble motors, unplugged) gets its error
// in dwStatus immediately, so the block is free again either way.
static void XBOXOG_FlushRumble(XBOXOG_Port *p)
{
    if (!p->rumble_pending || p->feedback.Header.dwStatus == ERROR_IO_PENDING) {
        return;
    }
    SDL_zero(p->feedback);
    p->feedback.Rumble.wLeftMotorSpeed = p->rumble_low;
    p->feedback.Rumble.wRightMotorSpeed = p->rumble_high;
    XInputSetState(p->device, &p->feedback);
    p->rumble_pending = false;
}

static bool XBOXOG_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    XBOXOG_Port *p = &ports[joystick->hwdata->port];

    if (!p->device) {
        return SDL_SetError("Controller is not connected");
    }
    // The left motor is the heavy, low-frequency one.
    p->rumble_low = low_frequency_rumble;
    p->rumble_high = high_frequency_rumble;
    p->rumble_pending = true;
    XBOXOG_FlushRumble(p);
    return true;
}

static bool XBOXOG_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool XBOXOG_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool XBOXOG_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool XBOXOG_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

// The face, Black and White buttons are pressure sensitive (0-255); treat them
// as pressed above the controller's specified crosstalk level.
static bool XBOXOG_AnalogButtonDown(BYTE value)
{
    return value > XINPUT_GAMEPAD_MAX_CROSSTALK;
}

static void XBOXOG_JoystickUpdate(SDL_Joystick *joystick)
{
    XBOXOG_Port *p = &ports[joystick->hwdata->port];
    XINPUT_STATE state;
    Uint64 timestamp;
    Uint8 hat = SDL_HAT_CENTERED;

    if (!p->device) {
        return;
    }
    XBOXOG_FlushRumble(p);

    if (XInputGetState(p->device, &state) != ERROR_SUCCESS) {
        return; // unplugged; XBOXOG_JoystickDetect reports the removal
    }

    timestamp = SDL_GetTicksNS();
    const XINPUT_GAMEPAD *pad = &state.Gamepad;

    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_A, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_A]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_B, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_B]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_X, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_X]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_Y, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_Y]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_BLACK, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_BLACK]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_WHITE, XBOXOG_AnalogButtonDown(pad->bAnalogButtons[XINPUT_GAMEPAD_WHITE]));
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_BACK, (pad->wButtons & XINPUT_GAMEPAD_BACK) != 0);
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_START, (pad->wButtons & XINPUT_GAMEPAD_START) != 0);
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_LEFT_THUMB, (pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0);
    SDL_SendJoystickButton(timestamp, joystick, XBOXOG_BUTTON_RIGHT_THUMB, (pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0);

    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        hat |= SDL_HAT_UP;
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        hat |= SDL_HAT_DOWN;
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        hat |= SDL_HAT_LEFT;
    }
    if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        hat |= SDL_HAT_RIGHT;
    }
    SDL_SendJoystickHat(timestamp, joystick, 0, hat);

    // XInput's Y axes point up; SDL's point down. ~y flips without overflowing at -32768.
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_LEFT_X, pad->sThumbLX);
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_LEFT_Y, (Sint16)~pad->sThumbLY);
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_RIGHT_X, pad->sThumbRX);
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_RIGHT_Y, (Sint16)~pad->sThumbRY);
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_LEFT_TRIGGER,
                         (Sint16)(((int)pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] * 257) - 32768));
    SDL_SendJoystickAxis(timestamp, joystick, XBOXOG_AXIS_RIGHT_TRIGGER,
                         (Sint16)(((int)pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] * 257) - 32768));
}

static void XBOXOG_JoystickClose(SDL_Joystick *joystick)
{
    SDL_free(joystick->hwdata);
    joystick->hwdata = NULL;
}

static void XBOXOG_JoystickQuit(void)
{
    for (int port = 0; port < XBOXOG_PORT_COUNT; ++port) {
        if (ports[port].device) {
            XInputClose(ports[port].device);
            ports[port].device = NULL;
        }
    }
}

static bool XBOXOG_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    // White and Black sit where the shoulder buttons later went, so they map to
    // left and right shoulder; the triggers are the analog L and R.
    *out = (SDL_GamepadMapping){
        .a = { EMappingKind_Button, XBOXOG_BUTTON_A },
        .b = { EMappingKind_Button, XBOXOG_BUTTON_B },
        .x = { EMappingKind_Button, XBOXOG_BUTTON_X },
        .y = { EMappingKind_Button, XBOXOG_BUTTON_Y },
        .back = { EMappingKind_Button, XBOXOG_BUTTON_BACK },
        .start = { EMappingKind_Button, XBOXOG_BUTTON_START },
        .leftstick = { EMappingKind_Button, XBOXOG_BUTTON_LEFT_THUMB },
        .rightstick = { EMappingKind_Button, XBOXOG_BUTTON_RIGHT_THUMB },
        .leftshoulder = { EMappingKind_Button, XBOXOG_BUTTON_WHITE },
        .rightshoulder = { EMappingKind_Button, XBOXOG_BUTTON_BLACK },
        .dpup = { EMappingKind_Hat, 0x01 },
        .dpdown = { EMappingKind_Hat, 0x04 },
        .dpleft = { EMappingKind_Hat, 0x08 },
        .dpright = { EMappingKind_Hat, 0x02 },
        .leftx = { EMappingKind_Axis, XBOXOG_AXIS_LEFT_X },
        .lefty = { EMappingKind_Axis, XBOXOG_AXIS_LEFT_Y },
        .rightx = { EMappingKind_Axis, XBOXOG_AXIS_RIGHT_X },
        .righty = { EMappingKind_Axis, XBOXOG_AXIS_RIGHT_Y },
        .lefttrigger = { EMappingKind_Axis, XBOXOG_AXIS_LEFT_TRIGGER },
        .righttrigger = { EMappingKind_Axis, XBOXOG_AXIS_RIGHT_TRIGGER },
    };
    return true;
}

SDL_JoystickDriver SDL_XBOXOG_JoystickDriver = {
    XBOXOG_JoystickInit,
    XBOXOG_JoystickGetCount,
    XBOXOG_JoystickDetect,
    XBOXOG_JoystickIsDevicePresent,
    XBOXOG_JoystickGetDeviceName,
    XBOXOG_JoystickGetDevicePath,
    XBOXOG_JoystickGetDeviceSteamVirtualGamepadSlot,
    XBOXOG_JoystickGetDevicePlayerIndex,
    XBOXOG_JoystickSetDevicePlayerIndex,
    XBOXOG_JoystickGetDeviceGUID,
    XBOXOG_JoystickGetDeviceInstanceID,
    XBOXOG_JoystickOpen,
    XBOXOG_JoystickRumble,
    XBOXOG_JoystickRumbleTriggers,
    XBOXOG_JoystickSetLED,
    XBOXOG_JoystickSendEffect,
    XBOXOG_JoystickSetSensorsEnabled,
    XBOXOG_JoystickUpdate,
    XBOXOG_JoystickClose,
    XBOXOG_JoystickQuit,
    XBOXOG_JoystickGetGamepadMapping
};

#endif // SDL_JOYSTICK_XBOXOG
