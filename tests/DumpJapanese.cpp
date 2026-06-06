#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../include/AudioProcessor.h"
#include "../include/LibraryData.h"
#include "../include/Phonemizer.h"
#include "../include/VoiceData.h"

int main(int argc, char* argv[]) {
    const char* csvPath = argc > 1 ? argv[1] : "japanese_samples.csv";

    using namespace SharpVox;

    VoiceData voice = VoiceData::baseline_voice();
    Phonemizer fe(
        LibraryData::dictionary,
        static_cast<size_t>(LibraryData::dictionarySize),
        [](const std::string& key, size_t& outSize) -> const uint8_t* {
            return LibraryData::FindSymbol(key.c_str(), outSize);
        });

    std::ifstream f(csvPath);
    if (!f) { std::fprintf(stderr, "Cannot open %s\n", csvPath); return 1; }

    std::string line;
    std::getline(f, line);  // skip header

    int issues = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string hiragana, ipa, meaning;
        std::getline(ss, hiragana, ',');
        std::getline(ss, ipa, ',');
        std::getline(ss, meaning, ',');

        auto sentences = fe.TextToSentenceTokens(hiragana);
        if (sentences.empty()) {
            std::printf("%-30s  ERROR: no sentences\n", hiragana.c_str());
            issues++;
            continue;
        }

        std::string phonStr;
        bool hasJP = false, hasNull = false;
        for (auto& [tokens, endPunct] : sentences) {
            for (auto& tok : tokens) {
                if (tok.Phon == AudioProcessor::_SIL_) continue;
                if (tok.Phon < 0 || tok.Phon >= 128) { hasNull = true; phonStr += "?? "; continue; }
                const char* name = AudioProcessor::PhonemeNamesTable[tok.Phon];
                if (!name) { hasNull = true; phonStr += "NULL "; continue; }
                phonStr += name;
                phonStr += ' ';
                if (tok.Phon >= AudioProcessor::_JP_A_) hasJP = true;
            }
        }
        if (!phonStr.empty() && phonStr.back() == ' ') phonStr.pop_back();

        const char* flag = hasNull ? " [WARN:null]" : "";
        std::printf("%-30s  |  %s%s\n", hiragana.c_str(), phonStr.c_str(), flag);
        if (hasNull) issues++;
    }

    std::printf("\n%d issue(s) found\n", issues);
    return issues > 0 ? 1 : 0;
}
