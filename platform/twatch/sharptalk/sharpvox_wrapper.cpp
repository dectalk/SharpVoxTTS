#include "sharpvox_wrapper.h"
#include "TtsEngine.h"
#include "LibraryData.h"
#include "VoicePresets.h"
#include <memory>

static std::unique_ptr<SharpVox::TtsEngine> s_engine;

extern "C" {

void sharpvox_init(void) {
    if (s_engine) return;

    auto symbolsTable = [](const std::string& key, size_t& sz) -> const uint8_t* {
        return SharpVox::LibraryData::FindSymbol(key.c_str(), sz);
    };

    s_engine = std::make_unique<SharpVox::TtsEngine>(
        SharpVox::LibraryData::dictionary,
        (size_t)SharpVox::LibraryData::dictionarySize,
        symbolsTable,
        22050
    );
}

namespace {
    struct SpeakCtx {
        sharpvox_buffer_cb cb;
        void* userdata;
        const SharpVox::PhonemeEvent* events;
        int32_t event_count;
        int32_t sample_pos;
    };

    int find_phoneme(const SharpVox::PhonemeEvent* ev, int32_t n, int32_t pos) {
        float t = (float)pos / 22050.0f;
        int phon = -1;
        for (int32_t i = 0; i < n; i++) {
            if (ev[i].TimeSeconds <= t) phon = ev[i].Phoneme;
            else break;
        }
        return phon;
    }
}

void sharpvox_speak(const char* text, sharpvox_buffer_cb on_buffer, void* userdata) {
    if (!s_engine) sharpvox_init();

    SpeakCtx ctx;
    ctx.cb = on_buffer;
    ctx.userdata = userdata;
    ctx.events = nullptr;
    ctx.event_count = 0;
    ctx.sample_pos = 0;

    s_engine->SpeakWithEvents(
        text,
        [](const int16_t* buf, int32_t len, void* ud) {
            auto* c = static_cast<SpeakCtx*>(ud);
            int phon = find_phoneme(c->events, c->event_count, c->sample_pos);
            c->cb(buf, len, phon, c->userdata);
            c->sample_pos += len;
        },
        [](const SharpVox::PhonemeEvent* ev, int32_t n, void* ud) {
            auto* c = static_cast<SpeakCtx*>(ud);
            c->events      = ev;
            c->event_count = n;
        },
        &ctx
    );
}

void sharpvox_terminate(void) {
    s_engine.reset();
}

}
