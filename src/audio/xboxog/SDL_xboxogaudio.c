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

#ifdef SDL_AUDIO_DRIVER_XBOXOG

// Playback through the Xbox's DirectSound (MCPX APU): one looping hardware
// buffer split into chunks; the audio thread fills the chunk after the one
// being played, like SDL's DirectSound driver on Windows.

#include "../SDL_sysaudio.h"
#include "../../core/xboxog/SDL_xboxog.h"

// Chunks in the looping buffer: enough to ride out a late audio thread
// without adding much latency.
#define XBOXOG_AUDIO_CHUNKS 4

struct SDL_PrivateAudioData
{
    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER buffer;
    DWORD chunk_count;
    DWORD last_chunk; // the chunk the play cursor was in when we last filled one
    Uint8 *locked;
};

static bool XBOXOGAUDIO_SetError(const char *what, HRESULT hr)
{
    return SDL_SetError("DirectSound %s failed (0x%08lx)", what, (unsigned long)hr);
}

static bool XBOXOGAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h;
    WAVEFORMATEX wfx;
    DSBUFFERDESC desc;
    HRESULT hr;

    h = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*h));
    if (!h) {
        return false;
    }
    device->hidden = h;

    // The APU mixes 8- and 16-bit PCM, mono or stereo on a plain buffer, at up
    // to 48 kHz; SDL converts anything else.
    device->spec.format = SDL_AUDIO_S16LE;
    if (device->spec.channels > 2) {
        device->spec.channels = 2;
    }
    if (device->spec.freq > 48000) {
        device->spec.freq = 48000;
    }
    SDL_UpdatedAudioDeviceFormat(device);

    hr = DirectSoundCreate(NULL, &h->dsound, NULL);
    if (FAILED(hr)) {
        return XBOXOGAUDIO_SetError("Create", hr);
    }

    SDL_zero(wfx);
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)device->spec.channels;
    wfx.nSamplesPerSec = (DWORD)device->spec.freq;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(wfx.nChannels * (wfx.wBitsPerSample / 8));
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    h->chunk_count = XBOXOG_AUDIO_CHUNKS;
    SDL_zero(desc);
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = (DWORD)device->buffer_size * h->chunk_count;
    desc.lpwfxFormat = &wfx;

    hr = IDirectSound_CreateSoundBuffer(h->dsound, &desc, &h->buffer, NULL);
    if (FAILED(hr)) {
        return XBOXOGAUDIO_SetError("CreateSoundBuffer", hr);
    }

    // Start from silence, so the chunks the cursor plays before the first fill are quiet.
    {
        void *p1 = NULL, *p2 = NULL;
        DWORD n1 = 0, n2 = 0;
        hr = IDirectSoundBuffer_Lock(h->buffer, 0, 0, &p1, &n1, &p2, &n2, DSBLOCK_ENTIREBUFFER);
        if (FAILED(hr)) {
            return XBOXOGAUDIO_SetError("Lock", hr);
        }
        SDL_memset(p1, device->silence_value, n1);
        if (p2) {
            SDL_memset(p2, device->silence_value, n2);
        }
        IDirectSoundBuffer_Unlock(h->buffer, p1, n1, p2, n2);
    }

    h->last_chunk = 0;
    hr = IDirectSoundBuffer_Play(h->buffer, 0, 0, DSBPLAY_LOOPING);
    if (FAILED(hr)) {
        return XBOXOGAUDIO_SetError("Play", hr);
    }
    return true;
}

static bool XBOXOGAUDIO_GetChunk(SDL_AudioDevice *device, DWORD *chunk)
{
    DWORD play = 0, write = 0;
    HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(device->hidden->buffer, &play, &write);
    if (FAILED(hr)) {
        return XBOXOGAUDIO_SetError("GetCurrentPosition", hr);
    }
    *chunk = play / (DWORD)device->buffer_size;
    return true;
}

static bool XBOXOGAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    // Wait for the play cursor to move into a new chunk, which frees the chunk after it.
    const Uint64 chunk_ns = SDL_SECONDS_TO_NS(device->sample_frames) / device->spec.freq;

    while (!SDL_GetAtomicInt(&device->shutdown)) {
        DWORD chunk;

        // Services deferred DirectSound work (notifications, streams, voice updates).
        DirectSoundDoWork();
        if (!XBOXOGAUDIO_GetChunk(device, &chunk)) {
            return false;
        }
        if (chunk != device->hidden->last_chunk) {
            break;
        }
        SDL_DelayNS(chunk_ns / 4);
    }
    return true;
}

static Uint8 *XBOXOGAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    DWORD chunk, n1 = 0, n2 = 0;
    void *p1 = NULL, *p2 = NULL;
    HRESULT hr;

    SDL_assert(*buffer_size == device->buffer_size);

    if (!XBOXOGAUDIO_GetChunk(device, &chunk)) {
        return NULL;
    }
    h->last_chunk = chunk;
    chunk = (chunk + 1) % h->chunk_count;

    // Chunks are buffer_size aligned, so the lock never wraps.
    hr = IDirectSoundBuffer_Lock(h->buffer, chunk * (DWORD)device->buffer_size, (DWORD)device->buffer_size, &p1, &n1,
                                 &p2, &n2, 0);
    if (FAILED(hr)) {
        XBOXOGAUDIO_SetError("Lock", hr);
        return NULL;
    }
    h->locked = (Uint8 *)p1;
    return h->locked;
}

static bool XBOXOGAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    HRESULT hr;

    SDL_assert(buflen == device->buffer_size);
    hr = IDirectSoundBuffer_Unlock(device->hidden->buffer, (LPVOID)buffer, (DWORD)buflen, NULL, 0);
    device->hidden->locked = NULL;
    if (FAILED(hr)) {
        return XBOXOGAUDIO_SetError("Unlock", hr);
    }
    return true;
}

static void XBOXOGAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h = device->hidden;

    if (!h) {
        return;
    }
    if (h->buffer) {
        IDirectSoundBuffer_Stop(h->buffer);
        IDirectSoundBuffer_Release(h->buffer);
    }
    if (h->dsound) {
        IDirectSound_Release(h->dsound);
    }
    SDL_free(h);
    device->hidden = NULL;
}

static void XBOXOGAUDIO_ThreadInit(SDL_AudioDevice *device)
{
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_HIGH);
}

static bool XBOXOGAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = XBOXOGAUDIO_OpenDevice;
    impl->ThreadInit = XBOXOGAUDIO_ThreadInit;
    impl->WaitDevice = XBOXOGAUDIO_WaitDevice;
    impl->GetDeviceBuf = XBOXOGAUDIO_GetDeviceBuf;
    impl->PlayDevice = XBOXOGAUDIO_PlayDevice;
    impl->CloseDevice = XBOXOGAUDIO_CloseDevice;
    impl->OnlyHasDefaultPlaybackDevice = true;
    impl->HasRecordingSupport = false;
    return true;
}

AudioBootStrap XBOXOGAUDIO_bootstrap = {
    "xboxog", "Xbox DirectSound", XBOXOGAUDIO_Init, false, false
};

#endif // SDL_AUDIO_DRIVER_XBOXOG
