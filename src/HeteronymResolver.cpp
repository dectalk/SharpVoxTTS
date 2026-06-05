#include "../include/HeteronymResolver.h"
#include "../include/AudioProcessor.h"
#include "../include/LibraryData.h"
#include <algorithm>
#include <cstdint>
#include <vector>
#include <string>
#include <initializer_list>

namespace SharpVox {

    // Contextual heteronym disambiguation.
    // For each ambiguous word we store a default pronunciation and a list of
    // context rules, the first matching rule wins.  CompiledLetterToSoundRules fire when a word in
    // the before-set appears immediately before OR a word in the after-set
    // appears immediately after the target.

    static const int16_t S1 = 56; // OP_STRESS1
    static const uint8_t OP_WORD_BYTE = 64; // word boundary marker (same as Phonemizer.OP_WORD)

    static std::vector<uint8_t> Ph(std::initializer_list<int16_t> p) {
        std::vector<uint8_t> buf;
        buf.reserve(p.size() + 1);
        buf.push_back(OP_WORD_BYTE);
        for (int16_t v : p) {
            buf.push_back(static_cast<uint8_t>(v));
        }
        return buf;
    }

    struct Rule {
        std::vector<std::string> Before;
        std::vector<std::string> After;
        std::vector<uint8_t> Phonemes;

        Rule(std::initializer_list<const char*> before,
             std::initializer_list<const char*> after,
             std::vector<uint8_t> ph)
            : Phonemes(std::move(ph))
        {
            Before.reserve(before.size());
            After.reserve(after.size());
            for (const char* s : before) Before.push_back(std::string(s));
            for (const char* s : after)  After.push_back(std::string(s));
        }

        bool Matches(const std::string* prev, const std::string* next) const {
            if (prev != nullptr && std::find(Before.begin(), Before.end(), *prev) != Before.end())
                return true;
            if (next != nullptr && std::find(After.begin(),  After.end(),  *next) != After.end())
                return true;
            return false;
        }
    };

    struct Entry {
        std::vector<Rule> CompiledLetterToSoundRules;
        std::vector<uint8_t> Default;
        Entry(std::vector<uint8_t> def, std::vector<Rule> rules)
            : CompiledLetterToSoundRules(std::move(rules)), Default(std::move(def)) {}
    };

    // LIVE
    static const std::vector<uint8_t> LiveVerb = Ph({AudioProcessor::_L_, S1, AudioProcessor::_IH_, AudioProcessor::_V_});         // /lIv/
    static const std::vector<uint8_t> LiveAdj  = Ph({AudioProcessor::_L_, S1, AudioProcessor::_AY_, AudioProcessor::_V_});         // /laIv/
    // READ
    static const std::vector<uint8_t> ReadPres = Ph({AudioProcessor::_R_, S1, AudioProcessor::_IY_, AudioProcessor::_D_});         // /ri:d/
    static const std::vector<uint8_t> ReadPast = Ph({AudioProcessor::_R_, S1, AudioProcessor::_EH_, AudioProcessor::_D_});         // /rEd/
    // LEAD
    static const std::vector<uint8_t> LeadVerb = Ph({AudioProcessor::_L_, S1, AudioProcessor::_IY_, AudioProcessor::_D_});         // /li:d/
    static const std::vector<uint8_t> LeadMet  = Ph({AudioProcessor::_L_, S1, AudioProcessor::_EH_, AudioProcessor::_D_});         // /lEd/
    // WIND
    static const std::vector<uint8_t> WindNoun = Ph({AudioProcessor::_W_, S1, AudioProcessor::_IH_, AudioProcessor::_N_, AudioProcessor::_D_});    // /wInd/
    static const std::vector<uint8_t> WindVerb = Ph({AudioProcessor::_W_, S1, AudioProcessor::_AY_, AudioProcessor::_N_, AudioProcessor::_D_});    // /waInd/
    // WOUND
    static const std::vector<uint8_t> WoundInj = Ph({AudioProcessor::_W_, S1, AudioProcessor::_UW_, AudioProcessor::_N_, AudioProcessor::_D_});    // /wu:nd/ injury
    static const std::vector<uint8_t> WoundPst = Ph({AudioProcessor::_W_, S1, AudioProcessor::_AW_, AudioProcessor::_N_, AudioProcessor::_D_});    // /waUnd/ past-of-wind
    // TEAR
    static const std::vector<uint8_t> TearRip  = Ph({AudioProcessor::_T_, S1, AudioProcessor::_EH_, AudioProcessor::_R_});         // /tEr/ rip
    static const std::vector<uint8_t> TearEye  = Ph({AudioProcessor::_T_, S1, AudioProcessor::_IH_, AudioProcessor::_R_});         // /tIr/ cry
    // BOW
    static const std::vector<uint8_t> BowWeap  = Ph({AudioProcessor::_B_, S1, AudioProcessor::_OW_});              // /boU/ weapon/ribbon
    static const std::vector<uint8_t> BowGest  = Ph({AudioProcessor::_B_, S1, AudioProcessor::_AW_});              // /baU/ gesture
    // CLOSE
    static const std::vector<uint8_t> CloseVrb = Ph({AudioProcessor::_K_, AudioProcessor::_L_, S1, AudioProcessor::_OW_, AudioProcessor::_Z_});    // /kloUz/ verb
    static const std::vector<uint8_t> CloseAdj = Ph({AudioProcessor::_K_, AudioProcessor::_L_, S1, AudioProcessor::_OW_, AudioProcessor::_S_});    // /kloUs/ near

    static std::vector<std::pair<std::string, Entry>> BuildTable() {
        std::vector<std::pair<std::string, Entry>> t;

        t.emplace_back("LIVE", Entry(LiveVerb, {
            Rule(
                {"GO", "GOES", "WENT", "STREAM", "STREAMED", "BROADCAST", "AIRED"},
                {"MUSIC", "CONCERT", "SHOW", "PERFORMANCE", "WIRE", "AMMUNITION",
                 "AMMO", "BAIT", "ROUND", "FIRE", "BROADCAST", "EVENT", "GAME", "GAMES"},
                LiveAdj)
        }));

        t.emplace_back("READ", Entry(ReadPres, {
            Rule(
                {"HAD", "HAVE", "HAS", "ALREADY", "JUST", "NEVER",
                 "I'VE", "YOU'VE", "WE'VE", "THEY'VE", "WHO'VE"},
                {},
                ReadPast)
        }));

        t.emplace_back("LEAD", Entry(LeadVerb, {
            Rule(
                {},
                {"PIPE", "PIPES", "PAINT", "PENCIL", "POISONING",
                 "BULLET", "BULLETS", "SHOT", "WEIGHT", "WEIGHTS", "FREE"},
                LeadMet)
        }));

        t.emplace_back("WIND", Entry(WindNoun, {
            Rule(
                {"TO", "WILL", "CAN", "COULD", "WOULD", "LET", "LETS"},
                {"UP", "DOWN", "BACK", "THROUGH", "AROUND"},
                WindVerb)
        }));

        t.emplace_back("WOUND", Entry(WoundInj, {
            Rule(
                {},
                {"UP", "DOWN", "BACK", "AROUND", "THROUGH"},
                WoundPst)
        }));

        t.emplace_back("TEAR", Entry(TearRip, {
            Rule(
                {"A", "THE", "ONE", "MY", "HER", "HIS", "YOUR",
                 "EACH", "EVERY", "SINGLE"},
                {"DUCT", "DUCTS", "DROP", "DROPS", "GAS", "JERKER", "STAINED"},
                TearEye)
        }));

        t.emplace_back("BOW", Entry(BowWeap, {
            Rule(
                {"TAKE", "TAKES", "TOOK", "MAKE", "MADE", "GIVE",
                 "GIVES", "GAVE", "DEEP"},
                {"DOWN", "TO", "BEFORE", "OUT"},
                BowGest)
        }));

        t.emplace_back("CLOSE", Entry(CloseVrb, {
            Rule(
                {"VERY", "TOO", "SO", "QUITE", "FAIRLY", "PRETTY",
                 "REALLY", "THAT", "THIS", "COME", "STAY", "REMAIN",
                 "REMAINS", "GET", "GETS", "CAME", "GETTING", "STAYING"},
                {"TO", "BY", "ENOUGH", "CALL", "FRIEND", "FRIENDS",
                 "CONTACT", "SHAVE", "QUARTERS", "TOGETHER"},
                CloseAdj)
        }));

        return t;
    }

    static const std::vector<std::pair<std::string, Entry>> Table = BuildTable();

    // Returns the full phoneme stream (OP_WORD + phonemes) if a rule matches,
    // or null to fall through to normal dictionary/LTS lookup.
    std::vector<uint8_t> HeteronymResolver::Resolve(const std::vector<std::string>& words, int32_t index) {
        const std::string& word = words[index];
        auto it = std::find_if(Table.begin(), Table.end(),
            [&word](const std::pair<std::string, Entry>& p) { return p.first == word; });
        if (it == Table.end()) {
            return {};
        }

        const Entry& entry = it->second;
        const std::string* prev = (index > 0) ? &words[index - 1] : nullptr;
        const std::string* next = (index < (int32_t)words.size() - 1) ? &words[index + 1] : nullptr;

        for (const Rule& rule : entry.CompiledLetterToSoundRules) {
            if (rule.Matches(prev, next)) {
                return rule.Phonemes;
            }
        }

        return entry.Default;
    }

}  // namespace SharpVox
