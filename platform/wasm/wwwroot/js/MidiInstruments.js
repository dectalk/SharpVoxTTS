// General MIDI instrument detection and voice+phoneme pairing for the MIDI
// converter. Pairings follow size and timbre: large/low instruments get the
// long-tract low voices, small/high ones the short-tract voices, bowed and
// reed tones nasals, round brass W/UW, breathy pipes the high-aspiration voices.
(function (global) {
  'use strict';

  const GM_NAMES = [
    'Acoustic Grand Piano', 'Bright Acoustic Piano', 'Electric Grand Piano', 'Honky-tonk Piano',
    'Electric Piano 1', 'Electric Piano 2', 'Harpsichord', 'Clavinet',
    'Celesta', 'Glockenspiel', 'Music Box', 'Vibraphone',
    'Marimba', 'Xylophone', 'Tubular Bells', 'Dulcimer',
    'Drawbar Organ', 'Percussive Organ', 'Rock Organ', 'Church Organ',
    'Reed Organ', 'Accordion', 'Harmonica', 'Tango Accordion',
    'Nylon Guitar', 'Steel Guitar', 'Jazz Guitar', 'Clean Guitar',
    'Muted Guitar', 'Overdriven Guitar', 'Distortion Guitar', 'Guitar Harmonics',
    'Acoustic Bass', 'Fingered Bass', 'Picked Bass', 'Fretless Bass',
    'Slap Bass 1', 'Slap Bass 2', 'Synth Bass 1', 'Synth Bass 2',
    'Violin', 'Viola', 'Cello', 'Contrabass',
    'Tremolo Strings', 'Pizzicato Strings', 'Orchestral Harp', 'Timpani',
    'String Ensemble 1', 'String Ensemble 2', 'Synth Strings 1', 'Synth Strings 2',
    'Choir Aahs', 'Voice Oohs', 'Synth Voice', 'Orchestra Hit',
    'Trumpet', 'Trombone', 'Tuba', 'Muted Trumpet',
    'French Horn', 'Brass Section', 'Synth Brass 1', 'Synth Brass 2',
    'Soprano Sax', 'Alto Sax', 'Tenor Sax', 'Baritone Sax',
    'Oboe', 'English Horn', 'Bassoon', 'Clarinet',
    'Piccolo', 'Flute', 'Recorder', 'Pan Flute',
    'Blown Bottle', 'Shakuhachi', 'Whistle', 'Ocarina',
    'Square Lead', 'Sawtooth Lead', 'Calliope Lead', 'Chiff Lead',
    'Charang Lead', 'Voice Lead', 'Fifths Lead', 'Bass + Lead',
    'New Age Pad', 'Warm Pad', 'Polysynth Pad', 'Choir Pad',
    'Bowed Pad', 'Metallic Pad', 'Halo Pad', 'Sweep Pad',
    'Rain FX', 'Soundtrack FX', 'Crystal FX', 'Atmosphere FX',
    'Brightness FX', 'Goblins FX', 'Echoes FX', 'Sci-fi FX',
    'Sitar', 'Banjo', 'Shamisen', 'Koto',
    'Kalimba', 'Bagpipe', 'Fiddle', 'Shanai',
    'Tinkle Bell', 'Agogo', 'Steel Drums', 'Woodblock',
    'Taiko Drum', 'Melodic Tom', 'Synth Drum', 'Reverse Cymbal',
    'Guitar Fret Noise', 'Breath Noise', 'Seashore', 'Bird Tweet',
    'Telephone Ring', 'Helicopter', 'Applause', 'Gunshot',
  ];

  // Default pairing per GM family (program >> 3), as [voice, phoneme].
  const FAMILY_PAIRS = [
    ['beth', 'AA'],     // piano
    ['tommy', 'IY'],    // chromatic percussion
    ['john', 'M'],      // organ
    ['jess', 'AE'],     // guitar
    ['matt', 'UW'],     // bass
    ['deborah', 'NG'],  // strings
    ['deborah', 'NG'],  // ensemble
    ['jack', 'AO'],     // brass
    ['jack', 'ER'],     // reed
    ['deborah', 'UW'],  // pipe
    ['chris', 'AE'],    // synth lead
    ['deborah', 'M'],   // synth pad
    ['jess', 'EH'],     // synth fx
    ['jess', 'NG'],     // ethnic
    ['tommy', 'IY'],    // percussive
    ['whisper', 'HH'],  // sound effects
  ];

  // Per-program overrides where the family default misses the register or timbre.
  const PROGRAM_PAIRS = {
    6: ['jess', 'IY'], 7: ['jess', 'IH'],
    12: ['beth', 'OW'], 14: ['beth', 'OW'],
    19: ['matt', 'M'], 21: ['jack', 'AA'], 22: ['jess', 'IY'], 23: ['jack', 'AA'],
    29: ['chris', 'AE'], 30: ['chris', 'AE'],
    35: ['matt', 'M'], 38: ['matt', 'AE'], 39: ['matt', 'AE'],
    40: ['tommy', 'NG'], 41: ['beth', 'NG'], 42: ['john', 'NG'], 43: ['matt', 'NG'],
    45: ['jess', 'AE'], 46: ['beth', 'IY'], 47: ['matt', 'AO'],
    52: ['jess', 'AA'], 53: ['beth', 'UW'], 54: ['jack', 'AA'], 55: ['john', 'AA'],
    56: ['chris', 'AA'], 57: ['john', 'AO'], 58: ['matt', 'W'], 59: ['chris', 'UW'],
    60: ['john', 'OW'], 62: ['chris', 'AA'], 63: ['chris', 'AA'],
    64: ['beth', 'ER'], 65: ['deborah', 'ER'], 67: ['matt', 'ER'],
    68: ['jess', 'N'], 69: ['deborah', 'N'], 70: ['matt', 'N'], 71: ['beth', 'UW'],
    72: ['tommy', 'UW'], 78: ['tommy', 'IY'], 79: ['beth', 'UW'],
    80: ['jack', 'EH'], 82: ['deborah', 'UW'], 83: ['jess', 'IY'],
    84: ['chris', 'AA'], 85: ['beth', 'AA'], 86: ['jack', 'AO'], 87: ['matt', 'AE'],
    91: ['jess', 'AA'], 92: ['deborah', 'NG'],
    105: ['jess', 'AE'], 106: ['beth', 'AE'], 107: ['beth', 'IY'], 108: ['tommy', 'UW'],
    109: ['jess', 'NG'], 110: ['tommy', 'NG'], 111: ['jess', 'N'],
    114: ['beth', 'OW'], 116: ['matt', 'AA'], 117: ['john', 'AO'], 118: ['jack', 'AO'],
    119: ['whisper', 'SH'],
  };

  // GM channel 10 (index 9) is percussion regardless of program.
  const DRUM_PAIR = ['whisper', 'CH'];

  // Name keywords to a representative GM program, most specific first, for
  // files that name their tracks but never send a program change.
  const NAME_KEYWORDS = [
    [/contra ?bass|double ?bass|upright bass/, 43], [/tuba|sousaphone/, 58],
    [/trombone/, 57], [/english horn|cor anglais/, 69], [/french horn|\bhorns?\b/, 60], [/muted trumpet/, 59],
    [/trumpet|cornet|flugel/, 56], [/brass/, 61],
    [/violin/, 40], [/viola/, 41], [/cello/, 42], [/fiddle/, 110],
    [/pizz/, 45], [/harp\b|harp$/, 46], [/timpani/, 47], [/strings?/, 48],
    [/choir|chorus|aah/, 52], [/ooh|vocal|voice|vox/, 53],
    [/soprano sax/, 64], [/alto sax/, 65], [/tenor sax/, 66], [/bari(tone)? sax/, 67], [/sax/, 65],
    [/oboe/, 68], [/english horn|cor anglais/, 69], [/bassoon/, 70], [/clarinet/, 71],
    [/piccolo/, 72], [/flute|fife/, 73], [/recorder/, 74], [/whistle/, 78], [/ocarina/, 79],
    [/bagpipe/, 109], [/harmonica/, 22], [/accordion/, 21],
    [/organ/, 16], [/harpsichord/, 6], [/clav/, 7], [/e(lectric)?\.? ?piano|rhodes/, 4], [/piano|keys/, 0],
    [/celesta/, 8], [/glock/, 9], [/music box/, 10], [/vibraphone|vibes/, 11],
    [/marimba/, 12], [/xylophone/, 13], [/bells?/, 14],
    [/synth ?bass/, 38], [/bass/, 33],
    [/distort|overdrive/, 30], [/guitar|gtr/, 25], [/banjo/, 105], [/sitar/, 104], [/koto/, 107],
    [/pad/, 89], [/lead|synth/, 81], [/drum|perc|kit/, -1],
  ];

  function pairForProgram(program) {
    return PROGRAM_PAIRS[program] || FAMILY_PAIRS[program >> 3];
  }

  function programFromName(name) {
    const lower = (name || '').toLowerCase();
    if (!lower) return null;
    for (const [re, prog] of NAME_KEYWORDS) {
      if (re.test(lower)) return prog;
    }
    return null;
  }

  // Resolve a channel's instrument from its program change, else from any
  // track or instrument name. Returns null when nothing identifies it.
  function detect(channel, program, names) {
    if (channel === 9) {
      return { name: 'Drum Kit', source: 'channel 10', voice: DRUM_PAIR[0], phoneme: DRUM_PAIR[1] };
    }
    if (program !== null && program !== undefined) {
      const [voice, phoneme] = pairForProgram(program);
      return { name: GM_NAMES[program], program, source: 'program', voice, phoneme };
    }
    for (const n of names || []) {
      const prog = programFromName(n);
      if (prog === null) continue;
      const [voice, phoneme] = prog < 0 ? DRUM_PAIR : pairForProgram(prog);
      return { name: prog < 0 ? 'Drums' : GM_NAMES[prog], program: prog < 0 ? null : prog,
        source: `name "${n}"`, voice, phoneme };
    }
    return null;
  }

  global.MidiInstruments = { GM_NAMES, detect, pairForProgram, programFromName };

})(window);
