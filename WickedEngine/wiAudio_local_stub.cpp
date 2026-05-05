#include "wiAudio.h"
#include "wiAudio_BindLua.h"

namespace wi::audio
{
void Initialize() {}

bool CreateSound(const std::string&, Sound* sound)
{
    if (sound != nullptr)
    {
        *sound = {};
    }
    return false;
}

bool CreateSound(const uint8_t*, size_t, Sound* sound)
{
    if (sound != nullptr)
    {
        *sound = {};
    }
    return false;
}

bool CreateSoundInstance(const Sound*, SoundInstance* instance)
{
    if (instance != nullptr)
    {
        *instance = {};
    }
    return false;
}

void Play(SoundInstance*) {}
void Pause(SoundInstance*) {}
void Stop(SoundInstance*) {}
void SetVolume(float, SoundInstance*) {}
float GetVolume(const SoundInstance*) { return 0.0f; }
void ExitLoop(SoundInstance*) {}
bool IsEnded(SoundInstance*) { return true; }
SampleInfo GetSampleInfo(const Sound*) { return {}; }
uint64_t GetTotalSamplesPlayed(const SoundInstance*) { return 0; }
void SetSubmixVolume(SUBMIX_TYPE, float) {}
float GetSubmixVolume(SUBMIX_TYPE) { return 0.0f; }
void Update3D(SoundInstance*, const SoundInstance3D&) {}
void SetReverb(REVERB_PRESET) {}
}

namespace wi::lua
{
void Audio_BindLua::Bind() {}
}
