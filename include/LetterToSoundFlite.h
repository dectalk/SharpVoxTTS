#ifndef SHARPVOX_LETTER_TO_SOUND_FLITE_H
#define SHARPVOX_LETTER_TO_SOUND_FLITE_H

// Adapted from the CMU flite speech synthesis system (festvox/flite v2.2):
// src/lexicon/cst_lts.c and include/cst_lts.h. Copyright (c) 1999 Language
// Technologies Institute, Carnegie Mellon University, BSD-style license; see
// THIRD_PARTY_NOTICES.md. Rewritten in C++ around this project's PhonemeDefs
// output; the .cpp documents the full list of modifications.

#include <cstdint>
#include <string>
#include <vector>

namespace SharpVox {

    // Rule-based letter-to-sound fallback built on the CMU flite decision-tree
    // LTS model (see THIRD_PARTY_NOTICES.md). This is a compiled decision tree,
    // not a statistical/neural model, and it runs entirely in-process.
    //
    // Pipeline position: dictionary lookup -> this model (primary fallback) ->
    // LetterToSoundNRL (secondary fallback, NRL Report 7948 rules). The NRL
    // rules run only when this path is compiled out, disabled, or emits nothing.
    //
    // The implementation is compiled in by default; defining
    // SHARPVOX_DISABLE_FLITE_LTS makes Convert() return false so the NRL path
    // handles every dictionary miss. SetEnabled() is a runtime rollback lever
    // on top of that.
    class LetterToSoundFlite {
    public:
        // Convert a word to a PhonemeDefs id sequence.
        //
        // The word is normalized exactly like the NRL path: case-folded to
        // lowercase and any character outside a-z dropped (flite's model only
        // understands lowercase a-z; the NRL rules ignore the same characters).
        //
        // phons  : one PhonemeDefs id per emitted phone, in order.
        // stress : parallel per-phone stress level (0 none, 1 primary). The
        //          compact CMU model carries no secondary-stress symbol, so this
        //          path never produces secondary stress.
        //
        // Returns false when the model is compiled out or disabled, or when the
        // word yields no phone; the caller should then try the NRL fallback.
        static bool Convert(const std::string& word,
                            std::vector<uint8_t>& phons,
                            std::vector<uint8_t>& stress);

        // Runtime rollback lever: enable/disable this fallback without rebuilding.
        static void SetEnabled(bool enabled);
        static bool Enabled();
    };

}  // namespace SharpVox

#endif  // SHARPVOX_LETTER_TO_SOUND_FLITE_H
