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

/* The test suites create and read back files relative to the current
   directory, as they do in a desktop build directory. An Xbox title starts in
   D:\, which is the read-only disc when run from an ISO, so run the suites
   from the title's persistent storage (T:\) instead. */

#include <unistd.h>

__attribute__((constructor)) static void xbox_testenv_set_cwd(void)
{
    chdir("T:\\");
}
