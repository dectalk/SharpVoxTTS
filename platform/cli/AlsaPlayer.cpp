#ifdef HAVE_ALSA

#include "AlsaPlayer.h"
#include <stdexcept>
#include <string>

namespace SharpVox {

AlsaPlayer::AlsaPlayer(int32_t sampleRate) : _pcm(nullptr) {
    int err = snd_pcm_open(&_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
        throw std::runtime_error(std::string("ALSA open: ") + snd_strerror(err));

    // If setup throws, the destructor won't run (object not fully
    // constructed), so close the open handle before rethrowing.
    try {
        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(_pcm, params);

        if ((err = snd_pcm_hw_params_set_access(_pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
            throw std::runtime_error(std::string("ALSA set access: ") + snd_strerror(err));
        if ((err = snd_pcm_hw_params_set_format(_pcm, params, SND_PCM_FORMAT_S16_LE)) < 0)
            throw std::runtime_error(std::string("ALSA set format: ") + snd_strerror(err));
        if ((err = snd_pcm_hw_params_set_channels(_pcm, params, 1)) < 0)
            throw std::runtime_error(std::string("ALSA set channels: ") + snd_strerror(err));

        unsigned int rate = static_cast<unsigned int>(sampleRate);
        if ((err = snd_pcm_hw_params_set_rate_near(_pcm, params, &rate, nullptr)) < 0)
            throw std::runtime_error(std::string("ALSA set rate: ") + snd_strerror(err));

        if ((err = snd_pcm_hw_params(_pcm, params)) < 0)
            throw std::runtime_error(std::string("ALSA hw_params: ") + snd_strerror(err));

        snd_pcm_prepare(_pcm);
    } catch (...) {
        snd_pcm_close(_pcm);
        _pcm = nullptr;
        throw;
    }
}

AlsaPlayer::~AlsaPlayer() {
    if (_pcm) {
        snd_pcm_drain(_pcm);
        snd_pcm_close(_pcm);
    }
    // Release alsa-lib's process-global config cache; reloaded on next open.
    snd_config_update_free_global();
}

void AlsaPlayer::Write(const int16_t* samples, int32_t count) {
    while (count > 0) {
        snd_pcm_sframes_t n = snd_pcm_writei(_pcm, samples, static_cast<snd_pcm_uframes_t>(count));
        if (n == -EPIPE) {
            snd_pcm_prepare(_pcm);
        } else if (n < 0) {
            snd_pcm_recover(_pcm, static_cast<int>(n), 0);
        } else {
            samples += n;
            count   -= static_cast<int32_t>(n);
        }
    }
}

}  // namespace SharpVox

#endif  // HAVE_ALSA
