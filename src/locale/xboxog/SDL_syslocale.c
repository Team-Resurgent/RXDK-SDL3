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
#include "../SDL_syslocale.h"
#include "../../core/xboxog/SDL_xboxog.h"

bool SDL_SYS_GetPreferredLocales(char *buf, size_t buflen)
{
    // The dashboard offers one language at a time; the region is not part of the setting.
    const char *locale;

    switch (XGetLanguage()) {
    case XC_LANGUAGE_JAPANESE:
        locale = "ja_JP";
        break;
    case XC_LANGUAGE_GERMAN:
        locale = "de_DE";
        break;
    case XC_LANGUAGE_FRENCH:
        locale = "fr_FR";
        break;
    case XC_LANGUAGE_SPANISH:
        locale = "es_ES";
        break;
    case XC_LANGUAGE_ITALIAN:
        locale = "it_IT";
        break;
    case XC_LANGUAGE_KOREAN:
        locale = "ko_KR";
        break;
    case XC_LANGUAGE_TCHINESE:
        locale = "zh_TW";
        break;
    case XC_LANGUAGE_PORTUGUESE:
        locale = "pt_PT";
        break;
    case XC_LANGUAGE_ENGLISH:
    default:
        locale = "en_US";
        break;
    }

    SDL_strlcpy(buf, locale, buflen);
    return true;
}
