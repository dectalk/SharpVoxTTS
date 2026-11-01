#ifndef SHARPVOX_PHONEME_DEFS_H
#define SHARPVOX_PHONEME_DEFS_H

#include <cstdint>

// Shared phoneme IDs, stream opcodes, control flags, and feature flags.
// Single source of truth; all tables across the engine are indexed by these values.

namespace SharpVox {

	// Phoneme IDs
	constexpr int16_t _IY_	= 0;
	constexpr int16_t _IH_	= 1; // close/near-close front
	constexpr int16_t _EH_	= 2;
	constexpr int16_t _AE_	= 3; // open-mid/near-open front
	constexpr int16_t _IX_	= 4;
	constexpr int16_t _AX_	= 5; // central: reduced-i, schwa
	constexpr int16_t _ER_	= 6;
	constexpr int16_t _AH_	= 7; // r-colored mid-central, open-mid back
	constexpr int16_t _AA_	= 8;
	constexpr int16_t _AO_	= 9; // open back, open-mid back rounded
	constexpr int16_t _UH_	= 10;
	constexpr int16_t _UW_	= 11; // near-close/close back rounded
	constexpr int16_t _EY_	= 12;
	constexpr int16_t _AY_	= 13; // diphthongs
	constexpr int16_t _OY_	= 14;
	constexpr int16_t _AW_	= 15;
	constexpr int16_t _OW_	= 16;
	constexpr int16_t _YU_	= 17;
	constexpr int16_t _IR_	= 18;
	constexpr int16_t _XR_	= 19; // r-colored vowels
	constexpr int16_t _AR_	= 20;
	constexpr int16_t _OR_	= 21;
	constexpr int16_t _UR_	= 22;
	constexpr int16_t _SIL_ = 23; // silence
	constexpr int16_t _M_	= 24;
	constexpr int16_t _N_	= 25; // nasals
	constexpr int16_t _NG_	= 26;
	constexpr int16_t _W_	= 27; // velar nasal; bilabial-velar approx.
	constexpr int16_t _Y_	= 28;
	constexpr int16_t _R_	= 29; // approximants
	constexpr int16_t _L_	= 30;
	constexpr int16_t _RX_	= 31; // lateral; syllabic r
	constexpr int16_t _LX_	= 32;
	constexpr int16_t _EL_	= 33; // syllabic consonants
	constexpr int16_t _EN_	= 34;
	constexpr int16_t _F_	= 35; // syllabic n; labiodental fric.
	constexpr int16_t _V_	= 36;
	constexpr int16_t _TH_	= 37; // fricatives
	constexpr int16_t _DH_	= 38;
	constexpr int16_t _S_	= 39;
	constexpr int16_t _Z_	= 40;
	constexpr int16_t _SH_	= 41;
	constexpr int16_t _ZH_	= 42;
	constexpr int16_t _HH_	= 43; // glottal fricative
	constexpr int16_t _P_	= 44;
	constexpr int16_t _B_	= 45; // plosives
	constexpr int16_t _T_	= 46;
	constexpr int16_t _D_	= 47;
	constexpr int16_t _K_	= 48;
	constexpr int16_t _G_	= 49;
	constexpr int16_t _CH_	= 50;
	constexpr int16_t _JH_	= 51; // affricates
	constexpr int16_t _TX_	= 52;
	constexpr int16_t _DX_	= 53; // allophones
	constexpr int16_t _QX_	= 54;
	constexpr int16_t _DD_	= 55;
	// Extra vowel target phonemes (no glide): English block 56-61
	constexpr int16_t _AYX_ = 56;
	constexpr int16_t _EYX_ = 57; // AY/EY targets
	constexpr int16_t _OYX_ = 58;
	constexpr int16_t _AWX_ = 59; // OY/AW targets
	constexpr int16_t _OWX_ = 60;
	constexpr int16_t _YUX_ = 61; // OW/YU targets
	// Japanese phonemes: block 62-66 (local indices 0-4 = JP_A..JP_O)
	constexpr int16_t _JP_A_     = 62;
	constexpr int16_t _JP_I_     = 63;
	constexpr int16_t _JP_U_     = 64;
	constexpr int16_t _JP_E_     = 65;
	constexpr int16_t _JP_O_     = 66;
	constexpr int16_t _Comma_    = 67;
	constexpr int16_t _Period_   = 68;
	constexpr int16_t _Quest_    = 69;
	constexpr int16_t _Exclam_   = 70;
	constexpr int16_t _Tilde_    = 71;
	constexpr int16_t _Ellipsis_ = 72;

	// Phoneme stream opcodes, byte-typed, shared by all files that build phoneme arrays
	constexpr uint8_t kOpStress1	= 56;
	constexpr uint8_t kOpStress2	= 57;
	constexpr uint8_t kOpEmphStress = 58;
	constexpr uint8_t kOpSyll	= 63;
	constexpr uint8_t kOpWord	= 64;
	constexpr uint8_t kOpPrep	= 65;
	constexpr uint8_t kOpVerb	= 66;
	constexpr uint8_t kOpComma	= 67;
	constexpr uint8_t kOpPeriod	= 68;
	constexpr uint8_t kOpQuest	= 69;
	constexpr uint8_t kOpExclam	= 70;

	// Control buffer flags for PhonemeToken::Ctrl
	constexpr int64_t kSyllableTypeField		  = 0x0F;
	constexpr int64_t kWord_End			  = 0x0001;
	constexpr int64_t kPrep_End			  = 0x0002;
	constexpr int64_t kVerb_End			  = 0x0004;
	constexpr int64_t kTerm_End			  = 0x0008;
	constexpr int64_t kWord_Initial_Consonant	  = 0x0080;
	constexpr int64_t kSyllableOrderField		  = 0x0300;
	constexpr int64_t kFirst_Syllable_In_Word	  = 0x0100;
	constexpr int64_t kMid_Syllable_In_Word		  = 0x0200;
	constexpr int64_t kLast_Syllable_In_Word	  = 0x0300;
	constexpr int64_t kMore_Than_One_Syllable_In_Word = 0x0300;
	constexpr int64_t kPrimaryStress		  = 0x0400;
	constexpr int64_t kSecondaryStress		  = 0x0800;
	constexpr int64_t kEmphaticStress		  = 0x1000;
	constexpr int64_t kStressField			  = 0x1C00;
	constexpr int64_t kIsStressed			  = 0x1C00;
	constexpr int64_t kPrimOrEmphStress		  = 0x1400;
	constexpr int64_t kContent_Word			  = 0x2000;
	constexpr int64_t kPronounWord			  = 0x80000000L;
	constexpr int64_t kBoundryTypeField		  = 0xF0000L;
	constexpr int64_t kWord_Start			  = 0x10000L;
	constexpr int64_t kPrep_Start			  = 0x20000L;
	constexpr int64_t kVerb_Start			  = 0x40000L;
	constexpr int64_t kTerm_Bound			  = 0x80000L;
	constexpr int64_t kSilenceTypeField		  = 0x00F00000L;
	constexpr int32_t kSilenceTypeShift		  = 20;
	constexpr int64_t kSilenceDuration		  = 0x01000000L;
	constexpr int64_t kSingingDuration		  = 0x40000000L;
	constexpr int64_t kSingingPhon			  = 0x20000000L;
	constexpr int64_t kSyllable_Start		  = 0x10000000L;
	constexpr int64_t kPitchRise			  = 0x0020L;
	constexpr int64_t kPitchFall			  = 0x0040L;
	constexpr int64_t kPitchRise1			  = 0x04000000L;
	constexpr int64_t kPitchFall1			  = 0x08000000L;
	constexpr int64_t kLowVibrato			  = 0x10L;
	constexpr int64_t kNoteDur			  = 0x0F00L;
	constexpr int32_t kNoteDurShift			  = 8;
	constexpr int64_t kNotePitch			  = 0x00FFL;
	constexpr int64_t kCompoundNoun			  = 0x8000L;
	constexpr int64_t kStressedWInitial		  = kIsStressed | kWord_Initial_Consonant;
	constexpr int64_t kSampleMarker			  = 0x02000000L;
	constexpr int64_t kJapaneseMora			  = 0x100000000LL;
	// content word that is usually left unaccented or weakly accented like light verbs, quantifiers, degree adverbs, set on every phoneme
	constexpr int64_t kLightContent	   = 0x200000000LL;
	constexpr int64_t kPlosive_Release = 0x4000;

	// BND types for silence, index into BoundaryDurationTable
	constexpr int32_t kBND_Pause	= 1;
	constexpr int32_t kBND_Decl	= 2;
	constexpr int32_t kBND_Quest	= 3;
	constexpr int32_t kBND_Emph	= 4;
	constexpr int32_t kBND_Ellipsis = 14;

	// Phoneme feature flags, layout of Tables::PhonemeFeatureFlags entries
	constexpr uint32_t kVowelF	 = 1u << 0;
	constexpr uint32_t kConsonantF	 = 1u << 1;
	constexpr uint32_t kVoicedF	 = 1u << 2;
	constexpr uint32_t kVowel1F	 = 1u << 3;
	constexpr uint32_t kSonorantF	 = 1u << 4;
	constexpr uint32_t kSonorant1F	 = 1u << 5;
	constexpr uint32_t kNasalF	 = 1u << 6;
	constexpr uint32_t kLiqGlideF	 = 1u << 7;
	constexpr uint32_t kSonorConsonF = 1u << 8;
	constexpr uint32_t kPlosiveF	 = 1u << 9;
	constexpr uint32_t kPlosFricF	 = 1u << 10;
	constexpr uint32_t kObstF	 = 1u << 11;
	constexpr uint32_t kStopF	 = 1u << 12;
	constexpr uint32_t kAlveolarF	 = 1u << 13;
	constexpr uint32_t kVelar	 = 1u << 14;
	constexpr uint32_t kLabialF	 = 1u << 15;
	constexpr uint32_t kDentalF	 = 1u << 16;
	constexpr uint32_t kPalatalF	 = 1u << 17;
	constexpr uint32_t kYGlideStartF = 1u << 18;
	constexpr uint32_t kYGlideEndF	 = 1u << 19;
	constexpr uint32_t kGStopF	 = 1u << 20;
	constexpr uint32_t kFrontF	 = 1u << 21;
	constexpr uint32_t kDiphthongF	 = 1u << 22;
	constexpr uint32_t kHasReleaseF	 = 1u << 23;
	constexpr uint32_t kAffricateF	 = 1u << 24;
	constexpr uint32_t kLiqGlide2F	 = 1u << 25;
	constexpr uint32_t kVocLiq	 = 1u << 26;
	constexpr uint32_t kFric	 = 1u << 27;

	// Pitch buffer event flags
	constexpr int16_t kPitchStress_Flg    = 0x1;
	constexpr int16_t kPitchRiseFall_Flg  = 0x2;
	constexpr int16_t kPitchBoundry_Flg   = 0x4;
	constexpr int16_t kResetDecline	      = 0x8;
	constexpr int16_t kPhraseReset	      = 0x10;
	constexpr int16_t kPitchRiseFall1_Flg = 0x20;

} // namespace SharpVox

#endif // SHARPVOX_PHONEME_DEFS_H
