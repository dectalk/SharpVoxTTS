#ifndef SHARPVOX_KLATTSCH_PARSER_H
#define SHARPVOX_KLATTSCH_PARSER_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace SharpVox {

// Forward declaration
struct PhonemeToken;

// Parser for Klattsch notation: a compact text format for phoneme-level singing and speech control.
// Directive state (current F0, rate, etc.) is per-instance; each TtsEngine owns one.
class KlattschParser {
public:
    struct Token {
        std::string Type;
        std::string Text;
        float       Ms        = 0.0f;
        std::string Key;
        float       Value     = 0.0f;
        bool        Relative  = false;
        bool        Reset     = false;
        std::string Code;
        bool        Stressed  = false;
        float       PitchDelta = 0.0f;
        bool        Transient = false;
        bool        Slurred   = false;
    };

    // Scan Klattsch source text into a flat Token list.
    // Handles: # line comments, /* block comments (including mid-token),
    // syllable-group parens ( ), pause punctuation , ; . and stress marks ! '.
    // Stress marks retroactively mark the most recent phoneme token as stressed.
    static std::vector<Token> Tokenize(const std::string& rawInput);

    // Convert a Klattsch token list to PhonemeTokens ready for AudioProcessor.
    //
    // Directives update persistent state (_curF0, _curRate, etc.) as they are encountered.
    // Phonemes outside syllable groups get full beat duration (_curRate ms, x1.5 if stressed).
    // Phonemes inside ( ) groups share the beat: stops get a short burst, others split the rest.
    // Pauses and p-directives emit SIL tokens with exact millisecond durations.
    // A trailing SIL with kTerm_End is always appended so AudioProcessor sees a clean clause end.
    std::vector<PhonemeToken> CompileToTokens(const std::vector<Token>& tokens);

    // Reset persistent Klattsch state to defaults.
    void Reset();
    void Reset(const struct VoiceData& voice);

    // Mapping from SharpVox phoneme ID -> Klattsch uppercase code (e.g. _IY_ -> "IY").
    static const std::unordered_map<int16_t, std::string>& GetPhonemeNamesTable();


private:
    // Persistent state for Klattsch mode
    float _curF0       = 120.0f;
    float _curRate     = 110.0f;
    float _curScale    = 1.0f;
    float _curVibDepth = 0.0f;
    float _curVibRate  = 5.0f;
    float _curTremDepth = 0.0f;
    float _curTremRate  = 5.0f;
    float _curAsp      = 0.0f;
    float _curTilt     = 0.0f;
    float _curEffort   = 0.5f;

    // Greek and Cyrillic characters that look identical to Latin letters in most fonts.
    // Notation files pasted from score editors or other systems may silently contain them.
    static const std::unordered_map<char32_t, char> HomoglyphMapTable;

    static const std::unordered_map<char, std::string> DirectiveKeyMap;

    // NFKC-normalize, strip zero-width characters (ZWSP, ZWNJ, ZWJ, WJ, BOM),
    // then replace any remaining homoglyphs so downstream parsing sees plain ASCII.
    static std::string Normalize(const std::string& input);

    static float NoteToHz(const std::string& name);

    // Classify one whitespace-delimited token from the Klattsch source.
    // Tries each shape in priority order: punctuation, bracket directives [KEY=N],
    // note names (bC4), compact directives (b120 r+10), phoneme codes (AE IY ...).
    // Returns nullptr if the token is syntactically valid but produces no output (e.g. bare "p").
    static Token* ClassifyPart(const std::string& part, Token& out);

    // Emit one phoneme token into result, using current persistent state.
    void EmitPhoneme(const Token& t, float durationMs,
                     bool isStartOfBeat, bool isEndOfBeat,
                     std::vector<PhonemeToken>& result);

    // Flush the current syllable group, distributing beat time across its phonemes.
    void FlushSyllable(std::vector<Token>& syllableQueue,
                       bool& inSyllable,
                       std::vector<PhonemeToken>& result);

    static const std::unordered_map<char, int32_t>      NoteSemitonesTable;
};

}  // namespace SharpVox

#endif  // SHARPVOX_KLATTSCH_PARSER_H
