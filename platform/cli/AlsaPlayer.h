#ifndef SHARPVOX_ALSA_PLAYER_H
#define SHARPVOX_ALSA_PLAYER_H

#ifdef HAVE_ALSA

#include <cstdint>
#include <alsa/asoundlib.h>

namespace SharpVox {

class AlsaPlayer {
public:
    AlsaPlayer(int32_t sampleRate);
    ~AlsaPlayer();

    AlsaPlayer(const AlsaPlayer&) = delete;
    AlsaPlayer& operator=(const AlsaPlayer&) = delete;

    void Write(const int16_t* samples, int32_t count);

private:
    snd_pcm_t* _pcm;
};

}  // namespace SharpVox

#endif  // HAVE_ALSA
#endif  // SHARPVOX_ALSA_PLAYER_H
