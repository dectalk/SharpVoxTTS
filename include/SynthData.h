#ifndef SHARPVOX_SYNTH_DATA_H
#define SHARPVOX_SYNTH_DATA_H

#include <cstdint>
#include <vector>
#include <array>

namespace SharpVox {

	// High-rate intelligibility. Janse et al. (2003) found that at extreme
	// speech rates uniform (linear) time-scaling is more intelligible than
	// the natural non-linear pattern, so these helpers blend the duration model
	// toward linear as the rate climbs. Frac is 0 at/below kStart and 1.0 (65536)
	// at/above kFull; between, the natural incompressible timing is kept.
	namespace RateLin {
		// kStart/kFull overridable via -D at build time; defaults are the shipped values.
#ifndef SVX_LIN_START
#define SVX_LIN_START 150
#endif
#ifndef SVX_LIN_FULL
#define SVX_LIN_FULL 350
#endif
		static constexpr int32_t kNormalRate = 180;	      // matches AudioProcessor baseline
		static constexpr int32_t kStart	     = SVX_LIN_START; // wpm: below this, natural timing
		static constexpr int32_t kFull	     = SVX_LIN_FULL;  // wpm: at/above this, fully linear

		// Q16 blend fraction in [0, 65536].
		static inline int32_t FracQ16(int32_t rate) {
			if(rate <= kStart) return 0;
			if(rate >= kFull) return 65536;
			return (int32_t)(((int64_t)(rate - kStart) << 16) / (kFull - kStart));
		}

		// Q16 multiplier for fixed-length events (formant transition ramps, stop
		// bursts) so they shrink toward the linear tempo ratio kNormalRate/rate at
		// high rates instead of keeping their absolute textbook length. 1.0 when
		// the blend is inactive.
		static inline int32_t EventScaleQ16(int32_t rate) {
			int32_t frac = FracQ16(rate);
			if(frac == 0 || rate <= 0) return 65536;
			int64_t ratioQ16 = ((int64_t)kNormalRate << 16) / rate;
			return (int32_t)(65536 + (((ratioQ16 - 65536) * frac) >> 16));
		}

		static inline uint64_t Isqrt64(uint64_t x) {
			uint64_t res = 0;
			uint64_t bit = (uint64_t)1 << 62;
			while(bit > x) bit >>= 2;
			while(bit != 0) {
				if(x >= res + bit) {
					x -= res + bit;
					res = (res >> 1) + bit;
				} else {
					res >>= 1;
				}
				bit >>= 2;
			}
			return res;
		}

		static inline int32_t PauseKeepQ16(int32_t rate) {
			if(rate <= kNormalRate || rate <= 0) return 65536;
			// sqrt(kNormalRate / rate) in Q16, via a Q32 ratio and integer sqrt.
			return (int32_t)Isqrt64(((int64_t)kNormalRate << 32) / rate);
		}
	} // namespace RateLin

	struct PitchState {
		int16_t NextPitchBufTime;
		int16_t PitchBufOutIndex;
		int16_t CurPitchBufTime;
		int16_t CurPitchBufPitch;
		int16_t CurPitchBufFlags;

		int16_t PhonIndexTarg;
		int16_t TimeIntoPhonTarg;
		int32_t CurPhonDurCc;
		int32_t PhonDurDelay;

		int16_t UvPhonPitchTarg;
		int16_t PhonPitchOffset;
		int16_t PhonPitchOffset1;

		int16_t BaseLineOffset;
		int16_t BasePitchOffset;

		int16_t BaselineFallStart;
		int16_t BaselineFallEnd;
		int16_t BaselineStartOffset;
		int16_t BaselineEndOffset;

		int64_t			 DownRampOffset;
		int64_t			 DownRampStep;
		std::array<int64_t, 256> RampSteps = {};
		int16_t			 CurRamp;

		int64_t VpIntonation;
		int64_t VpPitchRange;
		int16_t VpBaselinePitch;

		// Fujisaki intonation model knobs (Q16 neper amplitudes / percent scales)
		int64_t FujisakiPhraseAmpQ16;
		int64_t FujisakiPrimaryAccentAmpQ16;
		int64_t FujisakiHeadAccentAmpQ16;
		int64_t FujisakiSecondaryAccentAmpQ16;
		int64_t FujisakiEmphaticAccentAmpQ16;
		int32_t FujisakiPitchCurveExpPct;
		int32_t FujisakiMonoAccentDurScalePct;
		int64_t FujisakiCompoundStepQ16;
		int32_t FujisakiFinalDropPct;
		// index of this clause within the sentence (0 = sentence-initial)
		int16_t ClauseInSentence = 0;

		int64_t VibratoDepth1;
		int64_t VibratoDepth2;
		int64_t VibratoFreq;
		int32_t VibratoPhase1;

		int16_t Singing;
		int16_t HzGlide;
		int16_t MusicalNoteActive;
		int64_t PortamentoAccum;
		int64_t PortamentoStep;
		int16_t NewPortaTarget;
		int16_t NewSentence;
		int16_t SpeechRate;
	};

	struct ClausePlan {
		int32_t		     PhonBufInIndex;
		std::vector<int16_t> PhonBuf;
		std::vector<int64_t> PhonCtrlBuf;
		std::vector<int32_t> DurBuf;
		std::vector<int16_t> UserPitchBuf;
		std::vector<int16_t> UserNoteBuf;
		std::vector<uint8_t> AspirationBuf;
		std::vector<uint8_t> TiltBuf;
		std::vector<uint8_t> EffortBuf;
		std::vector<uint8_t> VibDepthBuf;
		std::vector<uint8_t> VibRateBuf;
		std::vector<uint8_t> TremDepthBuf;
		std::vector<uint8_t> TremRateBuf;

		uint32_t	     PitchBufInIndex;
		std::vector<int16_t> PitchBufFreq;
		std::vector<int16_t> PitchBufTime;
		std::vector<int16_t> PitchBufFlags;
		std::vector<int16_t> PitchBufDuration;

		// Clause-terminating punctuation (_Period_, _Quest_, ...; 0 = none)
		int16_t EndPunctuation = 0;

		PitchState Pitch;

		static ClausePlan Create(
		    int32_t		 phonBufInIndex,
		    std::vector<int16_t> phonBuf,
		    std::vector<int64_t> controls,
		    std::vector<int32_t> durBuf,
		    std::vector<int16_t> userPitchBuf,
		    std::vector<int16_t> userNoteBuf,
		    std::vector<uint8_t> aspirationBuf,
		    std::vector<uint8_t> tiltBuf,
		    std::vector<uint8_t> effortBuf,
		    std::vector<uint8_t> vibDepthBuf,
		    std::vector<uint8_t> vibRateBuf,
		    std::vector<uint8_t> tremDepthBuf,
		    std::vector<uint8_t> tremRateBuf,
		    uint32_t		 pitchBufInIndex,
		    std::vector<int16_t> pitchBufFreq,
		    std::vector<int16_t> pitchBufTime,
		    std::vector<int16_t> pitchBufFlags,
		    std::vector<int16_t> pitchBufDuration,
		    PitchState		 pitch,
		    int16_t		 endPunctuation) {
			ClausePlan d;
			d.PhonBufInIndex   = phonBufInIndex;
			d.PhonBuf	   = std::move(phonBuf);
			d.PhonCtrlBuf	   = std::move(controls);
			d.DurBuf	   = std::move(durBuf);
			d.UserPitchBuf	   = std::move(userPitchBuf);
			d.UserNoteBuf	   = std::move(userNoteBuf);
			d.AspirationBuf	   = std::move(aspirationBuf);
			d.TiltBuf	   = std::move(tiltBuf);
			d.EffortBuf	   = std::move(effortBuf);
			d.VibDepthBuf	   = std::move(vibDepthBuf);
			d.VibRateBuf	   = std::move(vibRateBuf);
			d.TremDepthBuf	   = std::move(tremDepthBuf);
			d.TremRateBuf	   = std::move(tremRateBuf);
			d.PitchBufInIndex  = pitchBufInIndex;
			d.PitchBufFreq	   = std::move(pitchBufFreq);
			d.PitchBufTime	   = std::move(pitchBufTime);
			d.PitchBufFlags	   = std::move(pitchBufFlags);
			d.PitchBufDuration = std::move(pitchBufDuration);
			d.EndPunctuation   = endPunctuation;
			d.Pitch		   = pitch;
			return d;
		}
	};

} // namespace SharpVox

#endif // SHARPVOX_SYNTH_DATA_H
