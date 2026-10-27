//  LetterToSoundFlite.cpp
//
//  Letter-to-sound fallback driven by the CMU flite decision-tree LTS model.
//
//  Adapted from the CMU flite speech synthesis system (festvox/flite v2.2),
//  files src/lexicon/cst_lts.c and include/cst_lts.h.
//  Copyright (c) 1999 Language Technologies Institute, Carnegie Mellon
//  University (author: Alan W Black). BSD-style permissive license; see
//  THIRD_PARTY_NOTICES.md.
//
//  Modifications (this port): rewritten in C++. flite's cst_val linked-list
//  output and its cst_alloc / cst_sprintf / cst_streq / cons_val utility layer
//  are replaced by std::string / std::vector; the model is read with portable
//  little-endian byte accesses instead of a struct memmove plus the
//  CST_BIG_ENDIAN byte swap; and the emitted phone symbols are mapped to this
//  project's PhonemeDefs ids (with a stress vector) instead of ARPAbet strings.

#include "../include/LetterToSoundFlite.h"
#include "../include/LetterToSoundFliteModel.h"
#include "../include/PhonemeDefs.h"

#include <cstring>
#include <string>
#include <vector>

namespace SharpVox {

    // Default state of the runtime rollback lever. The model is compiled in by
    // default; SHARPVOX_DISABLE_FLITE_LTS opts out at compile time.
#if !defined(SHARPVOX_DISABLE_FLITE_LTS)
    static bool s_fliteEnabled = true;
#else
    static bool s_fliteEnabled = false;
#endif

#if !defined(SHARPVOX_DISABLE_FLITE_LTS)

namespace {

    using FliteModel::CmuLtsLetterIndex;
    using FliteModel::CmuLtsModel;
    using FliteModel::CmuLtsPhoneTable;

    // Tree geometry, matching flite's cmu_lts_rules init in lang/cmulex/cmu_lex.c.
    constexpr int kContextWindowSize = 4;
    constexpr int kStateBytes = 6;       // 2 value bytes + two 2-byte addresses
    constexpr uint8_t kEndOfRule = 255;  // CST_LTS_EOR

    // Walk the decision tree from `start`, following the true/false branch at
    // each state on the context value for that feature index, and return the
    // phone-table index stored at the leaf.
    int ApplyModel(const uint8_t* vals, unsigned int start) {
        unsigned int addr = start;
        for (;;) {
            const uint8_t* st = CmuLtsModel + addr * kStateBytes;
            if (st[0] == kEndOfRule) {
                return st[1];
            }
            uint16_t qtrue = (uint16_t)(st[2] | (st[3] << 8));
            uint16_t qfalse = (uint16_t)(st[4] | (st[5] << 8));
            addr = (vals[st[0]] == st[1]) ? qtrue : qfalse;
        }
    }

    // Maps a flite base phone symbol (no stress digit) to a PhonemeDefs id.
    bool MapBasePhone(const std::string& base, uint8_t& id) {
        static constexpr struct { const char* name; uint8_t id; } kMap[] = {
            {"iy", (uint8_t)_IY_}, {"ih", (uint8_t)_IH_}, {"eh", (uint8_t)_EH_},
            {"ae", (uint8_t)_AE_}, {"ax", (uint8_t)_AX_}, {"ah", (uint8_t)_AH_},
            {"er", (uint8_t)_ER_}, {"aa", (uint8_t)_AA_}, {"ao", (uint8_t)_AO_},
            {"uh", (uint8_t)_UH_}, {"uw", (uint8_t)_UW_}, {"ey", (uint8_t)_EY_},
            {"ay", (uint8_t)_AY_}, {"oy", (uint8_t)_OY_}, {"aw", (uint8_t)_AW_},
            {"ow", (uint8_t)_OW_},
            {"b", (uint8_t)_B_},   {"ch", (uint8_t)_CH_}, {"d", (uint8_t)_D_},
            {"dh", (uint8_t)_DH_}, {"f", (uint8_t)_F_},   {"g", (uint8_t)_G_},
            {"hh", (uint8_t)_HH_}, {"jh", (uint8_t)_JH_}, {"k", (uint8_t)_K_},
            {"l", (uint8_t)_L_},   {"m", (uint8_t)_M_},   {"n", (uint8_t)_N_},
            {"ng", (uint8_t)_NG_}, {"p", (uint8_t)_P_},   {"r", (uint8_t)_R_},
            {"s", (uint8_t)_S_},   {"sh", (uint8_t)_SH_}, {"t", (uint8_t)_T_},
            {"th", (uint8_t)_TH_}, {"v", (uint8_t)_V_},   {"w", (uint8_t)_W_},
            {"y", (uint8_t)_Y_},   {"z", (uint8_t)_Z_},   {"zh", (uint8_t)_ZH_},
        };
        for (const auto& e : kMap) {
            if (base == e.name) {
                id = e.id;
                return true;
            }
        }
        return false;
    }

    // Maps one flite phone symbol (e.g. "aa1", "ax0", "b") to a PhonemeDefs id
    // and a stress level. flite only ever emits '0' (unstressed) or '1' (primary);
    // the compact CMU model has no secondary-stress symbol.
    //
    // The reduced schwa "ax" and the open-mid back vowel "ah" are the same sound
    // under flite/Festival's notation but two different phones in standard
    // ARPAbet and in this project's phoneset (whose dictionary uses _AX_ for the
    // reduced vowel and _AH_ for the stressed one). The fold is therefore done
    // per stress level: "ax0"/"ah0" -> _AX_, "ah1" -> _AH_. flite emits no
    // ax1/ax2/ah2, so no other cases arise.
    bool MapPhone(const std::string& sym, uint8_t& id, uint8_t& stress) {
        stress = 0;
        std::string base = sym;
        if (!base.empty() && (base.back() == '0' || base.back() == '1')) {
            stress = (base.back() == '1') ? 1 : 0;
            base.pop_back();
        }
        if (base == "ah") {
            id = (stress == 1) ? (uint8_t)_AH_ : (uint8_t)_AX_;
            return true;
        }
        return MapBasePhone(base, id);
    }

    // Appends the phoneme(s) named by one phone-table symbol. A symbol may name
    // two phones joined by '-' ("w-ey1", "t-s", ...); flite splits those at the
    // hyphen, and so do we, emitting the left phone then the right.
    void EmitPhone(const char* symbol, std::vector<uint8_t>& outPhons,
                   std::vector<uint8_t>& outStress) {
        const char* dash = strchr(symbol, '-');
        if (dash == nullptr) {
            uint8_t id = 0, stress = 0;
            if (MapPhone(symbol, id, stress)) {
                outPhons.push_back(id);
                outStress.push_back(stress);
            }
            return;
        }
        std::string parts[2] = {std::string(symbol, (size_t)(dash - symbol)),
                                std::string(dash + 1)};
        for (const std::string& part : parts) {
            uint8_t id = 0, stress = 0;
            if (MapPhone(part, id, stress)) {
                outPhons.push_back(id);
                outStress.push_back(stress);
            }
        }
    }

    // Lowercase and keep a-z only, mirroring how flite's model silently skips
    // characters it does not understand (and how the NRL path ignores them).
    void NormalizeWord(const std::string& word, std::string& out) {
        out.clear();
        out.reserve(word.size());
        for (char c : word) {
            if (c >= 'a' && c <= 'z') {
                out.push_back(c);
            } else if (c >= 'A' && c <= 'Z') {
                out.push_back((char)(c - 'A' + 'a'));
            }
        }
    }

}  // namespace

bool LetterToSoundFlite::Convert(const std::string& word,
                                 std::vector<uint8_t>& phons,
                                 std::vector<uint8_t>& stress) {
    phons.clear();
    stress.clear();

    if (!s_fliteEnabled) {
        return false;
    }

    std::string w;
    NormalizeWord(word, w);
    if (w.empty()) {
        return false;
    }

    // Pad with a '#' word boundary and (context_window_size - 1) zero-context
    // letters on each side, exactly as flite's lts_apply does.
    const int cw = kContextWindowSize;
    std::string fb;
    fb.reserve(w.size() + 2 * cw);
    fb.append((size_t)(cw - 1), '0');
    fb.push_back('#');
    fb += w;
    fb.push_back('#');
    fb.append((size_t)(cw - 1), '0');

    // Predict right-to-left (flite does this so the answer needs no reversal),
    // collecting each position's phone group in forward order. The groups come
    // out in reverse word order, so they are emitted from back to front.
    std::vector<std::vector<std::string>> groups;
    for (int pos = cw + (int)w.size() - 1; fb[(size_t)pos] != '#'; pos--) {
        char c = fb[(size_t)pos];
        if (c < 'a' || c > 'z') {
            continue;
        }
        int index = (c - 'a') % 26;

        // Context feature values: cw letters to the left, then cw to the right,
        // then the single always-zero extra (POS) feature slot.
        uint8_t vals[cw * 2 + 1];
        for (int k = 0; k < cw; k++) {
            vals[k] = (uint8_t)fb[(size_t)(pos - cw + k)];
            vals[cw + k] = (uint8_t)fb[(size_t)(pos + 1 + k)];
        }
        vals[cw * 2] = 0;

        int phone = ApplyModel(vals, CmuLtsLetterIndex[index]);
        const char* symbol = CmuLtsPhoneTable[phone];
        if (strcmp(symbol, "epsilon") == 0) {
            continue;
        }
        const char* dash = strchr(symbol, '-');
        if (dash != nullptr) {
            groups.push_back({std::string(symbol, (size_t)(dash - symbol)),
                              std::string(dash + 1)});
        } else {
            groups.push_back({std::string(symbol)});
        }
    }

    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        for (const std::string& symbol : *it) {
            EmitPhone(symbol.c_str(), phons, stress);
        }
    }

    return !phons.empty();
}

#else  // SHARPVOX_DISABLE_FLITE_LTS

bool LetterToSoundFlite::Convert(const std::string& word,
                                 std::vector<uint8_t>& phons,
                                 std::vector<uint8_t>& stress) {
    (void)word;
    phons.clear();
    stress.clear();
    return false;
}

#endif  // !SHARPVOX_DISABLE_FLITE_LTS

void LetterToSoundFlite::SetEnabled(bool enabled) {
#if !defined(SHARPVOX_DISABLE_FLITE_LTS)
    s_fliteEnabled = enabled;
#else
    (void)enabled;
#endif
}

bool LetterToSoundFlite::Enabled() {
    return s_fliteEnabled;
}

}  // namespace SharpVox
