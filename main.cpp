#include <cstdio>
#include "platform/lib/sharptalk_speaker.h"

using SharpTalk::SharpTalkSpeaker;

static void write_wav(const char* path, const short* buf, int n, int sr) {
    FILE* f = fopen(path, "wb");
    int data = n * 2;
    auto w32 = [f](int v)   { fwrite(&v, 4, 1, f); };
    auto w16 = [f](short v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 4, 1, f); w32(36 + data);
    fwrite("WAVEfmt ", 8, 1, f); w32(16); w16(1); w16(1);
    w32(sr); w32(sr * 2); w16(2); w16(16);
    fwrite("data", 4, 1, f); w32(data);
    fwrite(buf, 2, n, f);
    fclose(f);
}

int main() {
    SharpTalkSpeaker speaker;
    auto samples = speaker.Speak("hello world");
    write_wav("hello.wav", samples.data(), (int)samples.size(), speaker.SampleRate);
}
