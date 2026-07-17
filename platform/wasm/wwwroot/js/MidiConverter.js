// MIDI to klattsch converter built on the exact engine timing model: one 5ms
// request unit renders exactly one synth frame, so every voice is laid out
// open loop from t=0 and rendered in a single pass with no probe feedback.
(function (global) {
  'use strict';

  const NOTE_NAMES = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];

  // SharpVox duration tables mirrored from src/Tables.cpp, in milliseconds.
  const MIN_DURATION_MS = {
    IY: 60, IH: 60, EH: 60, AE: 50, AA: 50, AO: 50, AH: 90, UH: 70,
    UW: 90, ER: 100, AY: 50, AW: 50, EY: 110, OW: 100, OY: 110,
    W: 110, Y: 90, R: 130, L: 120, M: 120, N: 120, NG: 120, HH: 200,
    F: 60, TH: 35, S: 50, SH: 15, V: 30, DH: 30, Z: 40, ZH: 70,
    P: 70, B: 110, T: 100, D: 60, K: 55, G: 40, CH: 35, JH: 65,
    AX: 60, IX: 60, YU: 50, RX: 35, LX: 70, EL: 60, EN: 50, DX: 40,
    TX: 75, SIL: 65, JP_A: 100, JP_I: 70, JP_U: 50, JP_E: 20, JP_O: 50,
  };

  const FRAME_MS = 5;
  const UNIT_REQUEST_MS = 5;

  // The engine parses pause/duration ms into an int16, so chunks stay
  // at or below 30000ms (6000 units).
  const MAX_PAUSE_UNITS = 6000;

  // Legato overlaps up to this long are trimmed off the previous note
  // instead of forcing the next note into a separate voice.
  const OVERLAP_TRIM_MS = 80;

  // One vowel or diphthong per MIDI channel index, ordered so adjacent
  // channels contrast in height and backness, with short-minimum vowels
  // on the low channels and the drum channel (9).
  const CHANNEL_VOWELS = [
    'AW', 'IY', 'AO', 'EH', 'UW', 'AY', 'OW', 'AE',
    'EY', 'AA', 'UH', 'OY', 'IH', 'ER', 'AH', 'YU',
  ];

  function vowelForChannel(channel) {
    return CHANNEL_VOWELS[((channel % CHANNEL_VOWELS.length) + CHANNEL_VOWELS.length) % CHANNEL_VOWELS.length];
  }

  // MIDI file parsing

  function readVlq(view, pos) {
    let value = 0, n = 0;
    while (n < 4) {
      const b = view.getUint8(pos + n); n++;
      value = (value << 7) | (b & 0x7F);
      if (!(b & 0x80)) break;
    }
    return { value, n };
  }

  function midiToNoteName(note) {
    return NOTE_NAMES[note % 12] + (Math.floor(note / 12) - 1);
  }

  function parseTrack(view, start, len) {
    const events = [];
    let pos = start, absTime = 0, lastStatus = 0;
    const end = start + len;

    while (pos < end && pos < view.byteLength) {
      const { value: delta, n: dn } = readVlq(view, pos);
      pos += dn;
      absTime += delta;
      if (pos >= end) break;

      const b = view.getUint8(pos);

      if (b === 0xFF) {
        pos++;
        const metaType = view.getUint8(pos++);
        const { value: mlen, n: mln } = readVlq(view, pos);
        pos += mln;
        if (metaType === 0x51 && mlen >= 3) {
          const us = (view.getUint8(pos) << 16) | (view.getUint8(pos+1) << 8) | view.getUint8(pos+2);
          events.push({ absTime, type: 'tempo', tempo: us });
        }
        pos += mlen;

      } else if (b === 0xF0 || b === 0xF7) {
        pos++;
        const { value: slen, n: sln } = readVlq(view, pos);
        pos += sln + slen;
        lastStatus = 0;

      } else {
        let status;
        if (b & 0x80) { status = b; lastStatus = b; pos++; }
        else           { status = lastStatus; }

        const type = (status >> 4) & 0xF;
        const ch   = status & 0xF;

        switch (type) {
          case 0x8: {
            const note = view.getUint8(pos++), vel = view.getUint8(pos++);
            events.push({ absTime, type: 'note_off', ch, note, vel });
            break;
          }
          case 0x9: {
            const note = view.getUint8(pos++), vel = view.getUint8(pos++);
            events.push({ absTime, type: vel > 0 ? 'note_on' : 'note_off', ch, note, vel });
            break;
          }
          case 0xA: pos += 2; break; // aftertouch
          case 0xB: {
            const ctrl = view.getUint8(pos++), val = view.getUint8(pos++);
            events.push({ absTime, type: 'cc', ch, ctrl, val });
            break;
          }
          case 0xC: pos++;   break; // program change
          case 0xD: pos++;   break; // channel pressure
          case 0xE: pos += 2; break; // pitch bend
          default:  pos++;   break;
        }
      }
    }
    return events;
  }

  function buildTempoMap(allEvents) {
    const map = [{ tick: 0, tempo: 500000 }];
    for (const ev of allEvents) {
      if (ev.type !== 'tempo') continue;
      if (map[map.length - 1].tick === ev.absTime)
        map[map.length - 1].tempo = ev.tempo;
      else
        map.push({ tick: ev.absTime, tempo: ev.tempo });
    }
    return map;
  }

  function ticksToMs(startTick, endTick, tempoMap, tpb) {
    if (startTick >= endTick) return 0;
    let totalMs = 0, segStart = startTick, ti = 0;
    for (let i = 0; i < tempoMap.length; i++) {
      if (tempoMap[i].tick <= segStart) ti = i; else break;
    }
    while (segStart < endTick) {
      const { tempo } = tempoMap[ti];
      const nextTick  = ti + 1 < tempoMap.length ? tempoMap[ti + 1].tick : endTick;
      const segEnd    = Math.min(endTick, nextTick);
      totalMs += (segEnd - segStart) * (tempo / 1000.0 / tpb);
      segStart = segEnd;
      if (ti + 1 < tempoMap.length && tempoMap[ti + 1].tick <= segStart) ti++;
    }
    return totalMs;
  }

  function parseMidiBuffer(arrayBuffer) {
    const view = new DataView(arrayBuffer);

    const magic = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
    if (magic !== 'MThd') throw new Error('Not a valid MIDI file');

    const headerLen = view.getUint32(4);
    const ntrks     = view.getUint16(10);
    const division  = view.getUint16(12);
    if (division & 0x8000) throw new Error('SMPTE timecode MIDI not supported');
    const tpb = division;

    const allEvents = [];
    let pos = 8 + headerLen;
    for (let t = 0; t < ntrks; t++) {
      if (pos + 8 > view.byteLength) break;
      const tm = String.fromCharCode(view.getUint8(pos), view.getUint8(pos+1), view.getUint8(pos+2), view.getUint8(pos+3));
      const tlen = view.getUint32(pos + 4);
      pos += 8;
      if (tm === 'MTrk') allEvents.push(...parseTrack(view, pos, tlen));
      pos += tlen;
    }

    allEvents.sort((a, b) => a.absTime - b.absTime);

    const tempoMap = buildTempoMap(allEvents);

    const active       = new Map();
    const modWheel     = new Map();
    const chVolume     = new Map();
    const chExpression = new Map();
    const chEvents     = new Map();

    for (const ev of allEvents) {
      if (ev.type === 'cc') {
        if (ev.ctrl === 1) modWheel.set(ev.ch, ev.val);
        else if (ev.ctrl === 7) chVolume.set(ev.ch, ev.val);
        else if (ev.ctrl === 11) chExpression.set(ev.ch, ev.val);
      } else if (ev.type === 'note_on') {
        // Capture channel volume and expression at note start, GM defaults
        // CC7=100 and CC11=127.
        const vol  = chVolume.has(ev.ch) ? chVolume.get(ev.ch) : 100;
        const expr = chExpression.has(ev.ch) ? chExpression.get(ev.ch) : 127;
        active.set(`${ev.ch},${ev.note}`, { startTick: ev.absTime, vel: ev.vel, vol, expr });
      } else if (ev.type === 'note_off') {
        const key = `${ev.ch},${ev.note}`;
        if (active.has(key)) {
          const { startTick, vel, vol, expr } = active.get(key);
          active.delete(key);
          if (!chEvents.has(ev.ch)) chEvents.set(ev.ch, []);
          chEvents.get(ev.ch).push({
            ch: ev.ch, note: ev.note, vel,
            startTick, endTick: ev.absTime,
            durationMs: ticksToMs(startTick, ev.absTime, tempoMap, tpb),
            mod: modWheel.get(ev.ch) || 0,
            // GM/DLS square-law amplitude from velocity, volume, expression.
            gain: Math.pow((vel * vol * expr) / (127 * 127 * 127), 2),
          });
        }
      }
    }

    for (const evs of chEvents.values()) evs.sort((a, b) => a.startTick - b.startTick);

    return { tpb, tempoMap, chEvents };
  }

  // Voice splitting

  // Convert raw channel note events into notes with absolute millisecond times.
  function enrichNotes(evs, tempoMap, tpb) {
    return evs.map(ev => ({
      note: ev.note,
      vel: ev.vel,
      mod: ev.mod,
      gain: ev.gain !== undefined ? ev.gain : 1,
      startMs: ticksToMs(0, ev.startTick, tempoMap, tpb),
      endMs: ticksToMs(0, ev.endTick, tempoMap, tpb),
    }));
  }

  // Split a channel's notes into monophonic voices. Chords, overlaps beyond
  // the trim tolerance, and notes denser than the engine minimum phoneme
  // duration go to parallel voices that get mixed back together.
  function splitIntoVoices(notes, minAdvanceMs) {
    const voices = [];
    for (const n of notes) {
      let placed = false;
      for (const v of voices) {
        const last = v[v.length - 1];
        if (n.startMs - last.startMs < minAdvanceMs) continue;
        const overlapMs = last.endMs - n.startMs;
        if (overlapMs <= 0) {
          v.push(n);
          placed = true;
          break;
        }
        if (overlapMs <= OVERLAP_TRIM_MS && n.startMs > last.startMs) {
          last.endMs = n.startMs;
          v.push(n);
          placed = true;
          break;
        }
      }
      if (!placed) voices.push([n]);
    }
    return voices;
  }

  // Open-loop layout (exact engine timing model)

  // Samples rendered per 5ms unit: KlattSynthesizerFP sets SampFrameLen =
  // max(2, floor(rate * 112/22050 + 0.5)), and one durBuf unit renders one
  // frame. 244 samples (5.0833ms) at 48kHz, 112 (5.0794ms) at 22050.
  function unitSamples(sampleRate) {
    return Math.max(2, Math.floor(sampleRate * 112 / 22050 + 0.5));
  }

  // Emit a unit count as chained pause tokens, each under the int16 limit.
  function leadPauseTokens(units) {
    const tokens = [];
    let left = units;
    while (left > 0) {
      const chunk = Math.min(left, MAX_PAUSE_UNITS);
      tokens.push(`p${chunk * UNIT_REQUEST_MS}`);
      left -= chunk;
    }
    return tokens;
  }

  // Build a klattsch token stream from per-note unit durations and gaps.
  function buildTokens(notes, gapUnits, durUnits, phoneme, leadUnits) {
    const tokens = leadPauseTokens(leadUnits);
    let runNote = null;
    let runR = null;
    let runV = 0;

    for (let i = 0; i < notes.length; i++) {
      const n = notes[i];
      if (i > 0 && gapUnits[i] > 0) tokens.push(...leadPauseTokens(gapUnits[i]));

      const r = durUnits[i] * UNIT_REQUEST_MS;
      if (runR !== r) {
        tokens.push(`r${r}`);
        runR = r;
      }

      const vib = n.mod > 0 ? Math.round((n.mod / 127.0) * 8.0) : 0;
      if (vib !== runV) {
        tokens.push(`v${vib}`);
        runV = vib;
      }

      if (n.note !== runNote) {
        tokens.push(`b${midiToNoteName(n.note)}`);
        runNote = n.note;
      }

      tokens.push(n.phoneme || phoneme);
    }
    return tokens;
  }

  function minDurationUnits(phoneme) {
    const ms = Math.max(MIN_DURATION_MS[phoneme] || FRAME_MS, FRAME_MS);
    return Math.max(1, Math.ceil(ms / UNIT_REQUEST_MS));
  }

  // Engine minimum units for each note, honoring per-note phoneme overrides.
  function noteMinUnits(notes, phoneme) {
    return notes.map(n => minDurationUnits(n.phoneme || phoneme));
  }

  // Split an inter-onset span into note duration plus optional pause units,
  // keeping the note close to its nominal length. Any leftover of at least
  // one unit becomes a pause; pauses have no sub-unit minimum to avoid.
  function splitSpan(spanUnits, nominalUnits, minUnits) {
    const dur = Math.min(Math.max(minUnits, nominalUnits), spanUnits);
    return { dur, gap: spanUnits - dur };
  }

  // Exact open-loop layout of a whole voice from t=0. Spans come from exact
  // per-note target onsets in whole units, so cumulative rounding error never
  // exceeds half a unit and no probe feedback is needed. The initial SIL slot
  // always renders 1 unit; leading pauses make up the rest of the start time.
  // Returns tokens plus the exact engine onset of every note in samples.
  function layoutVoice(notes, phoneme, sampleRate) {
    if (!notes.length) throw new Error('layoutVoice: empty voice');
    const us = unitSamples(sampleRate);
    const unitMs = (us * 1000) / sampleRate;
    const minUnits = noteMinUnits(notes, phoneme);
    const n = notes.length;

    const firstUnits = Math.max(1, Math.round(notes[0].startMs / unitMs));
    const leadUnits = firstUnits - 1;

    const durUnits = new Array(n);
    const gapUnits = new Array(n).fill(0);
    const onsetUnits = new Array(n);
    onsetUnits[0] = firstUnits;

    for (let i = 0; i < n; i++) {
      const nominal = notes[i].endMs - notes[i].startMs;
      // Cap at MAX_PAUSE_UNITS so the r token stays inside the parser int16.
      const nominalUnits = Math.min(
        Math.max(minUnits[i], Math.round(nominal / unitMs)),
        MAX_PAUSE_UNITS
      );
      if (i + 1 < n) {
        const targetNext = Math.round(notes[i + 1].startMs / unitMs);
        const spanUnits = Math.max(minUnits[i], targetNext - onsetUnits[i]);
        const { dur, gap } = splitSpan(spanUnits, nominalUnits, minUnits[i]);
        durUnits[i] = dur;
        gapUnits[i + 1] = gap;
        onsetUnits[i + 1] = onsetUnits[i] + spanUnits;
      } else {
        durUnits[i] = nominalUnits;
      }
    }

    // The parser appends a 150ms terminating SIL (30 units) to every stream.
    const endUnits = onsetUnits[n - 1] + durUnits[n - 1];
    return {
      tokens: buildTokens(notes, gapUnits, durUnits, phoneme, leadUnits),
      unitMs,
      unitSamples: us,
      onsetUnits,
      onsetSamples: onsetUnits.map(u => u * us),
      totalSamples: (endUnits + 30) * us,
    };
  }

  // Mixing

  // Limiter target level; headroom up to 32767 absorbs smoothing overshoot.
  const MIX_KNEE = 30000;
  const MIX_CEIL = 32767;
  const LOOKAHEAD_MS = 5;
  const ATTACK_MS = 2;
  const RELEASE_MS = 80;

  function clamp16(v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return v;
  }

  // Safety shaper for residual overshoot past the limiter: unity slope at
  // the knee, bends smoothly toward MIX_CEIL instead of hard clamping.
  function softClip(v) {
    const a = Math.abs(v);
    if (a <= MIX_KNEE) return v;
    const room = MIX_CEIL - MIX_KNEE;
    const shaped = MIX_KNEE + room * Math.tanh((a - MIX_KNEE) / room);
    return v < 0 ? -shaped : shaped;
  }

  // Per-sample gain needed to keep |v| at or below the knee, pulled earlier
  // by a sliding-window minimum over the lookahead. Monotonic deque, O(n).
  function lookaheadGain(mix, lookahead) {
    const n = mix.length;
    const need = new Float64Array(n);
    for (let i = 0; i < n; i++) {
      const a = Math.abs(mix[i]);
      need[i] = a > MIX_KNEE ? MIX_KNEE / a : 1;
    }
    const out = new Float64Array(n);
    const deque = new Int32Array(n);
    let head = 0;
    let tail = 0;
    for (let i = 0; i < n; i++) {
      while (tail > head && need[deque[tail - 1]] >= need[i]) tail--;
      deque[tail++] = i;
      const winStart = i - lookahead;
      if (deque[head] < winStart) head++;
      if (i >= lookahead) out[i - lookahead] = need[deque[head]];
    }
    for (let i = Math.max(0, n - lookahead); i < n; i++) {
      while (deque[head] < i) head++;
      out[i] = need[deque[head]];
    }
    return out;
  }

  // Apply the per-note gain envelope (short linear ramps at note boundaries
  // to avoid clicks) while accumulating into target buffers, so no full
  // length intermediate copy is materialized per voice. mix is the master
  // Float64 accumulator; chBuf is an optional per-channel Float32.
  function accumulateWithGain(samples, boundarySamples, gains, sampleRate, mix, chBuf) {
    const n = Math.min(samples.length, mix.length);
    const ramp = Math.max(1, Math.round(0.003 * sampleRate));
    let bi = 0;
    for (let i = 0; i < n; i++) {
      while (bi + 1 < boundarySamples.length && i >= boundarySamples[bi + 1]) bi++;
      let g = gains[bi];
      if (bi + 1 < boundarySamples.length) {
        const toNext = boundarySamples[bi + 1] - i;
        if (toNext < ramp) {
          g += (gains[bi + 1] - g) * ((ramp - toNext) / ramp);
        }
      }
      const v = samples[i] * g;
      mix[i] += v;
      if (chBuf) chBuf[i] += v;
    }
  }

  // Run the float mix through a lookahead peak limiter: ride gain down ahead
  // of loud stacks and release it after, so sparse passages keep full level
  // and dense ones compress smoothly instead of clipping. Quiet mixes are
  // normalized up instead. Soft clip remains as a final safety only.
  function finalizeMix(mix, sampleRate) {
    const maxLen = mix.length;
    let peak = 0;
    for (let i = 0; i < maxLen; i++) {
      const a = Math.abs(mix[i]);
      if (a > peak) peak = a;
    }

    const out = new Int16Array(maxLen);
    if (peak <= MIX_KNEE) {
      // Quiet mix (e.g. velocity-scaled): normalize up to the target level.
      const up = peak > 0 ? Math.min(16, MIX_KNEE / peak) : 1;
      for (let i = 0; i < maxLen; i++) out[i] = Math.round(mix[i] * up);
      return { samples: out, peak, upScale: up, minGain: 1, limitedFraction: 0 };
    }

    const lookahead = Math.max(1, Math.round((LOOKAHEAD_MS / 1000) * sampleRate));
    const attCoef = Math.exp(-1 / ((ATTACK_MS / 1000) * sampleRate));
    const relCoef = Math.exp(-1 / ((RELEASE_MS / 1000) * sampleRate));
    const target = lookaheadGain(mix, lookahead);

    let g = target[0];
    let minGain = 1;
    let limited = 0;
    for (let i = 0; i < maxLen; i++) {
      const t = target[i];
      // Attack pole when gain must fall, release pole when it may recover.
      const coef = t < g ? attCoef : relCoef;
      g = t + (g - t) * coef;
      if (g < minGain) minGain = g;
      if (g < 0.999) limited++;
      out[i] = clamp16(Math.round(softClip(mix[i] * g)));
    }

    return {
      samples: out,
      peak,
      upScale: 1,
      minGain,
      limitedFraction: maxLen ? limited / maxLen : 0,
    };
  }

  // Public API

  let _parsed = null;

  function parse(arrayBuffer) {
    try {
      _parsed = parseMidiBuffer(arrayBuffer);
      const channels = [];
      for (const [ch, evs] of [..._parsed.chEvents.entries()].sort(([a],[b]) => a - b)) {
        if (!evs.length) continue;
        const startMs = ticksToMs(0, evs[0].startTick, _parsed.tempoMap, _parsed.tpb);
        const durMs = ticksToMs(0, evs[evs.length-1].endTick, _parsed.tempoMap, _parsed.tpb);
        channels.push({
          id: ch,
          label: `Channel ${ch}${ch === 9 ? ' [drums]' : ''} — ${evs.length} notes, start +${(startMs/1000).toFixed(2)}s, span ~${(durMs/1000).toFixed(2)}s`,
          noteCount: evs.length,
        });
      }
      return { channels, tpb: _parsed.tpb };
    } catch (e) {
      _parsed = null;
      return { error: e.message, channels: [] };
    }
  }

  // Plan every renderable voice for the selected channel(s). Each voice is a
  // self-contained token stream from t=0, with exact engine onsets for the
  // per-note gain envelope and the longest predicted length for the mix.
  function buildVoices(channelId, phoneme, sampleRate) {
    if (!_parsed) return null;
    // Default is one vowel per channel index like klattschmidi; a typed
    // phoneme forces that phoneme on every channel.
    phoneme = (phoneme || '').trim().toUpperCase();
    const forcedPhoneme = phoneme && phoneme !== 'AUTO' ? phoneme : null;

    const targets = channelId !== 'all'
      ? [parseInt(channelId, 10)]
      : [..._parsed.chEvents.keys()].sort((a, b) => a - b);

    const voices = [];
    for (const ch of targets) {
      const evs = _parsed.chEvents.get(ch);
      if (!evs || !evs.length) continue;
      const chPhoneme = forcedPhoneme || vowelForChannel(ch);
      const minAdvanceMs = Math.max(MIN_DURATION_MS[chPhoneme] || FRAME_MS, FRAME_MS);
      const notes = enrichNotes(evs, _parsed.tempoMap, _parsed.tpb);
      splitIntoVoices(notes, minAdvanceMs).forEach((voiceNotes, vi) => {
        const layout = layoutVoice(voiceNotes, chPhoneme, sampleRate);
        voices.push({
          channel: ch,
          voice: vi,
          name: `ch${ch}-v${vi}`,
          phoneme: chPhoneme,
          notes: voiceNotes,
          layout,
          text: `[:klattsch on] ${layout.tokens.join(' ')}`,
        });
      });
    }
    if (!voices.length) return null;

    return {
      voices,
      sampleRate,
      masterLen: voices.reduce((m, v) => Math.max(m, v.layout.totalSamples), 0),
      channels: [...new Set(voices.map(v => v.channel))].sort((a, b) => a - b),
    };
  }

  // Token text download: one self-contained voice per line from t=0
  function tokensFileText(job, fileName) {
    const lines = [
      `# polyphony klattsch tokens for ${fileName || 'midi'}`,
      '',
    ];

    var voice = 0;
    for (const v of job.voices) {
      lines.push(`# ${v.name} [${v.phoneme}] notes=${v.notes.length}`, `[voice=${voice}]`,v.text.replace("[:klattsch on]",""), '');
      voice = voice + 1;
    }

    return lines.join('\n');
  }

  function buildWav(samples, sampleRate) {
    const dataSize = samples.length * 2;
    const buf = new ArrayBuffer(44 + dataSize);
    const view = new DataView(buf);
    const ascii = (off, s) => { for (let i = 0; i < s.length; i++) view.setUint8(off + i, s.charCodeAt(i)); };
    ascii(0, 'RIFF'); view.setUint32(4, 36 + dataSize, true);
    ascii(8, 'WAVE'); ascii(12, 'fmt ');
    view.setUint32(16, 16, true);
    view.setUint16(20, 1, true);  view.setUint16(22, 1, true);
    view.setUint32(24, sampleRate, true);
    view.setUint32(28, sampleRate * 2, true);
    view.setUint16(32, 2, true);  view.setUint16(34, 16, true);
    ascii(36, 'data'); view.setUint32(40, dataSize, true);
    new Int16Array(buf, 44).set(samples);
    return new Uint8Array(buf);
  }

  global.MidiConverter = {
    parse,
    buildVoices,
    tokensFileText,
    accumulateWithGain,
    finalizeMix,
    buildWav,
    unitSamples,
    vowelForChannel,
  };

})(window);
