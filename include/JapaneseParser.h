#ifndef SHARPVOX_JAPANESE_PARSER_H
#define SHARPVOX_JAPANESE_PARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include "AudioProcessor.h"

namespace SharpVox {

    // Converts a UTF-8 hiragana span directly to PhonemeToken objects.
    // Vowels use JP phoneme IDs (56-60); consonants use English IDs.
    // Japanese /r/ uses DX (alveolar tap).
    class JapaneseParser {
    public:
        static std::vector<PhonemeToken> SpanToPhonemes(const std::string& text,
                                                         size_t pos, size_t len);
    };

}  // namespace SharpVox

#endif  // SHARPVOX_JAPANESE_PARSER_H
