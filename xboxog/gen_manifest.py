#!/usr/bin/env python3
"""Generate rxdk.project.json, the RXDK build manifest for the SDL3 static library.

The RXDK manifest does not expand globs, so the source list is generated here from
the subsystem directories. Re-run after adding or removing source files:

    python xboxog/gen_manifest.py
"""

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

# Directories whose top-level .c files are the platform-independent core.
CORE_DIRS = [
    "",
    "atomic",
    "audio",
    "camera",
    "core",
    "cpuinfo",
    "dialog",
    "dynapi",
    "events",
    "filesystem",
    "gpu",
    "haptic",
    "hidapi",
    "io",
    "joystick",
    "libm",
    "locale",
    "main",
    "misc",
    "power",
    "process",
    "render",
    "render/software",
    "sensor",
    "stdlib",
    "storage",
    "thread",
    "time",
    "timer",
    "tray",
    "video",
    "video/yuv2rgb",
]

# Upstream generic, dummy and POSIX backends this platform uses.
BACKEND_DIRS = [
    "audio/dummy",
    "camera/dummy",
    "dialog/dummy",
    "filesystem/posix",
    "haptic/dummy",
    "io/generic",
    "joystick/virtual",
    "loadso/dummy",
    "main/generic",
    "misc/dummy",
    "process/dummy",
    "sensor/dummy",
    "storage/generic",
    "time/unix",
    "tray/dummy",
    "video/dummy",
    "video/offscreen",
]

# Individual upstream files from directories that are otherwise platform-specific.
BACKEND_FILES = [
    "thread/generic/SDL_syscond.c",
    "thread/generic/SDL_sysrwlock.c",
]

# Upstream files in the core directories that do not apply to this platform.
EXCLUDE = {
    "misc/SDL_libusb.c",
    "video/yuv2rgb/yuv_rgb_lsx.c",
}


def c_files(rel_dir):
    full = os.path.join(SRC, rel_dir)
    for name in sorted(os.listdir(full)):
        if name.endswith(".c") and os.path.isfile(os.path.join(full, name)):
            rel = f"{rel_dir}/{name}" if rel_dir else name
            if rel not in EXCLUDE:
                yield rel


def xboxog_dirs():
    for dirpath, dirnames, _ in os.walk(SRC):
        dirnames.sort()
        if os.path.basename(dirpath) == "xboxog":
            yield os.path.relpath(dirpath, SRC).replace(os.sep, "/")


def main():
    sources = []
    for d in CORE_DIRS + BACKEND_DIRS + sorted(xboxog_dirs()):
        sources.extend(c_files(d))
    sources.extend(BACKEND_FILES)
    sources = ["src/" + s for s in sources]

    manifest = {
        "name": "SDL3",
        "type": "library",
        "configuration": "release",
        "sources": sources,
        "publicIncludePaths": ["include"],
        "includePaths": ["src", "include/build_config"],
        # Merged into the final link of any title referencing SDL3. libd3d8 and
        # libdsound are C++ inside, so the C++ runtime comes along.
        "libraries": ["libd3d8.lib", "libdsound.lib", "libxapi.lib", "libcpp.lib", "libc.lib", "libkernel.lib"],
        "defines": ["NDEBUG"],
        "compileFlags": ["-Wno-pragma-pack"],
        "outputDir": "out",
    }

    write_manifest(os.path.join(ROOT, "rxdk.project.json"), manifest)
    write_testautomation_manifest()


def write_manifest(path, manifest):
    with open(path, "w", newline="\n") as fh:
        json.dump(manifest, fh, indent=2)
        fh.write("\n")
    print(f"wrote {os.path.relpath(path, ROOT)} ({len(manifest['sources'])} sources)")


def write_testautomation_manifest():
    """SDL's own regression suite (test/testautomation*.c) as an XBE, built
    against the library project above."""
    here = os.path.join(ROOT, "xboxog", "tests", "testautomation")
    rel = os.path.relpath(ROOT, here).replace(os.sep, "/")
    tests = sorted(n for n in os.listdir(os.path.join(ROOT, "test"))
                   if n.startswith("testautomation") and n.endswith(".c"))
    framework = sorted(n for n in os.listdir(os.path.join(ROOT, "src", "test")) if n.endswith(".c"))
    manifest = {
        "name": "SDLTestAutomation",
        "configuration": "release",
        "sources": [f"{rel}/test/{n}" for n in tests] + [f"{rel}/src/test/{n}" for n in framework]
                   + ["xbox_testenv.c"],
        "includePaths": [f"{rel}/test"],
        "projectReferences": [rel],
        "compileFlags": ["-Wno-pragma-pack"],
        "outputDir": "out",
        "createIso": True,
        "imageBuild": {
            "stackSize": 262144,
            "debug": True,
            "noLogo": True,
            "noLibWarn": True,
            "testId": "0x5344b001",
            "testName": "SDLTestAutomation",
        },
    }
    os.makedirs(here, exist_ok=True)
    write_manifest(os.path.join(here, "rxdk.project.json"), manifest)


if __name__ == "__main__":
    main()
