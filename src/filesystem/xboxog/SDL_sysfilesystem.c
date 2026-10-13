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

#ifdef SDL_FILESYSTEM_XBOXOG

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
// System dependent filesystem routines

#include "../SDL_sysfilesystem.h"

#include <errno.h>
#include <sys/stat.h>

/* XAPI maps D: to the directory the XBE was launched from (the disc, or the
   title's folder on the hard disk -- RXDK-Libs maps the latter since the
   SDK release that made XapiInitProcess follow XeImageFileName), T: to the
   title's persistent storage on the hard disk, and U: to the current user's
   saved-game storage. D: is read-only when running from disc. */
#define XBOXOG_BASE_PATH "D:\\"
#define XBOXOG_PREF_ROOT "T:\\"
#define XBOXOG_SAVE_ROOT "U:\\"

char *SDL_SYS_GetBasePath(void)
{
    return SDL_strdup(XBOXOG_BASE_PATH);
}

char *SDL_SYS_GetExeName(void)
{
    return SDL_strdup(XBOXOG_BASE_PATH "default.xbe");
}

static bool MakeDirectory(const char *path)
{
    if (mkdir(path, 0777) == -1 && errno != EEXIST) {
        return SDL_SetError("Couldn't create directory '%s': %s", path, strerror(errno));
    }
    return true;
}

char *SDL_SYS_GetPrefPath(const char *org, const char *app)
{
    // T: is already private to this title, but homebrew often shares a title ID,
    // so still separate by org and app.
    char *path = NULL;

    if (*org) {
        if (SDL_asprintf(&path, "%s%s\\", XBOXOG_PREF_ROOT, org) < 0) {
            return NULL;
        }
        if (!MakeDirectory(path)) {
            SDL_free(path);
            return NULL;
        }
        SDL_free(path);
        if (SDL_asprintf(&path, "%s%s\\%s\\", XBOXOG_PREF_ROOT, org, app) < 0) {
            return NULL;
        }
    } else {
        if (SDL_asprintf(&path, "%s%s\\", XBOXOG_PREF_ROOT, app) < 0) {
            return NULL;
        }
    }

    if (!MakeDirectory(path)) {
        SDL_free(path);
        return NULL;
    }
    return path;
}

char *SDL_SYS_GetUserFolder(SDL_Folder folder)
{
    switch (folder) {
    case SDL_FOLDER_HOME:
        return SDL_strdup(XBOXOG_BASE_PATH);
    case SDL_FOLDER_SAVEDGAMES:
        return SDL_strdup(XBOXOG_SAVE_ROOT);
    default:
        SDL_Unsupported();
        return NULL;
    }
}

#endif // SDL_FILESYSTEM_XBOXOG
