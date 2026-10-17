# RXDK SDL3

<p align="center"><b>SDL 3 for the original Xbox — video, audio, controllers, threads and files on the console, built with the open RXDK SDK</b></p>

<p align="center">
  <a href="https://github.com/Team-Resurgent/RXDK-SDL3/blob/main/LICENSE.txt"><img src="https://img.shields.io/badge/License-zlib-blue.svg" alt="License: zlib"></a>
  <a href="https://discord.gg/VcdSfajQGK"><img src="https://img.shields.io/badge/chat-on%20discord-7289da.svg?logo=discord" alt="Discord"></a>
</p>

<p align="center">
  <a href="https://ko-fi.com/J3J7L5UMN"><img src="https://img.shields.io/badge/ko--fi-Support-FF5E5B?style=for-the-badge&logo=ko-fi&logoColor=white" alt="ko-fi"></a>
  <a href="https://www.patreon.com/teamresurgent"><img src="https://img.shields.io/badge/Patreon-F96854?style=for-the-badge&logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

This is [SDL 3.4.18](https://www.libsdl.org/) with an original Xbox (2001) platform, `xboxog`, added. A title
written against SDL's API builds into an XBE with the RXDK toolchain and runs on the console and on xemu.
[LEGO Island (RXDK-LEGO-Island)](https://github.com/Team-Resurgent/RXDK-LEGO-Island) uses it for its original Xbox port.

## What works

| Subsystem | Xbox backend |
|---|---|
| Video | Direct3D 8: one full-screen window in a mode from the console's video settings, with SDL's software renderer and window framebuffers on top |
| Audio | DirectSound: 16-bit PCM, up to stereo at 48 kHz |
| Joystick / gamepad | XInput: four controller ports, hot-plug, a gamepad mapping, rumble |
| Threads, mutexes, semaphores, TLS | Kernel threads and critical sections |
| Timers | XAPI's performance counter |
| Filesystem | The console's drives (see below) |
| Locale, time | The dashboard's language and time zone settings |

Not available on this platform: SDL's GPU API, HIDAPI, cameras, processes, loadable objects, dialogs and the
tray. Mouse and keyboard are not supported yet.

## Platform notes

- `SDL_PLATFORM_XBOXOG` is defined for this target. The compiler targets `i686-pc-windows-gnu`, so `_WIN32` is
  predefined for C, but there is no Win32 user-mode API: this platform is deliberately not
  `SDL_PLATFORM_WINDOWS`. `SDL_GetPlatform()` returns `"Xbox"`.
- **Display modes** come from the console's video settings: 640x480 and 720x480 at 60 Hz (progressive when 480p
  is enabled), 640x576 and 720x576 at 50 Hz on PAL (plus 480-line PAL-60), and 1280x720 and 1920x1080 when 720p
  or 1080i is enabled.
- **Direct3D 8 device:** the window's `SDL_PROP_WINDOW_XBOXOG_D3DDEVICE_POINTER` property is the console's
  `IDirect3DDevice8`. A title can render with Direct3D 8 itself instead of through an `SDL_Renderer`.
- **Paths:** `SDL_GetBasePath()` is `D:\`, the folder the title was launched from. `SDL_GetPrefPath()` is under
  `T:\` (the title's persistent storage on the hard disk), separated by organization and application.
  `SDL_GetUserFolder(SDL_FOLDER_SAVEDGAMES)` is `U:\`.
- **Controllers:** A, B, X, Y, Black and White are pressure sensitive on the console. SDL reports them as buttons.
  Black and White map to the shoulder buttons, and the triggers are axes.
- **main():** SDL provides `main()` (including with `SDL_MAIN_USE_CALLBACKS`). An Xbox title has no OS to return
  to, so when the app's main function finishes, the console returns to the dashboard.

## Building

SDL builds with the RXDK engine from `rxdk.project.json`, as a static library:

```bash
rxdk build --project-root .
```

The manifest's source list is generated. Re-run this after adding or removing SDL source files:

```bash
python xboxog/gen_manifest.py
```

## Using it in a title

Reference this repository from the title's `rxdk.project.json`. SDL's include directory is added automatically:

```json
{
  "name": "mygame",
  "sources": ["main.c"],
  "projectReferences": ["../RXDK-SDL3"],
  "libraries": ["libd3d8.lib", "libdsound.lib", "libxapi.lib", "libcpp.lib", "libc.lib", "libkernel.lib"]
}
```

## Tests

`xboxog/tests/testautomation` builds SDL's `testautomation` suite as an Xbox title:

```bash
rxdk build --project-root xboxog/tests/testautomation
```

Known failures: relative mouse mode (no mouse support yet), and two IOStream tests that open a file with
exclusive create (`"wx"`), which the console's C library does not yet refuse for an existing file.

## Upstream and license

SDL is by Sam Lantinga and the SDL contributors: <https://www.libsdl.org/>, <https://github.com/libsdl-org/SDL>.
See [INSTALL.md](INSTALL.md) and [docs](docs) for SDL itself.

SDL and the Xbox port are distributed under the terms of the zlib license, in [LICENSE.txt](LICENSE.txt).
