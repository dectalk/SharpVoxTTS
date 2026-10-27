#ifndef SHARPVOX_LETTER_TO_SOUND_FLITE_MODEL_H
#define SHARPVOX_LETTER_TO_SOUND_FLITE_MODEL_H

// Data tables for the flite-derived letter-to-sound decision tree.
//
// Adapted from the CMU flite speech synthesis system (festvox/flite v2.2):
// lang/cmulex/cmu_lts_model.c/.h and lang/cmulex/cmu_lts_rules.c. Copyright
// (c) 1999-2017 Language Technologies Institute, Carnegie Mellon University
// (BSD-style license). See LetterToSoundFlite.h for the engine and
// THIRD_PARTY_NOTICES.md for the upstream copyright and license terms.
// The tables were moved into a namespace and compiled as C++; the numeric
// contents are unchanged from upstream.
//
// The tables are defined in src/LetterToSoundFliteModel.cpp and are compiled
// unless SHARPVOX_DISABLE_FLITE_LTS is defined.

#if !defined(SHARPVOX_DISABLE_FLITE_LTS)

namespace SharpVox {
namespace FliteModel {

    // Decision-tree states, 6 bytes each: feature index, comparison value,
    // little-endian "true" address, little-endian "false" address. A feature
    // index of 255 (CST_LTS_EOR) marks a leaf whose value byte is a phone index.
    extern const unsigned char CmuLtsModel[];

    // First state address for each initial letter a-z (27 entries, sentinel last).
    extern const unsigned short CmuLtsLetterIndex[];

    // Phone symbol table, indexed by the leaf value. "epsilon" emits nothing; a
    // symbol containing '-' names two phones to split at the hyphen (e.g. "w-ey1").
    extern const char* const CmuLtsPhoneTable[];

}  // namespace FliteModel
}  // namespace SharpVox

#endif  // !SHARPVOX_DISABLE_FLITE_LTS

#endif  // SHARPVOX_LETTER_TO_SOUND_FLITE_MODEL_H
