// JS port of dec2klattsch.py, convert DECtalk phoneme notation to klattsch TXT
(function (global) {
  'use strict';

  const DT_TO_ARPABET = {
    'iy':'IY','ih':'IH','ey':'EY','eh':'EH','ae':'AE',
    'aa':'AA','ah':'AH','ao':'AO','ow':'OW','uh':'UH',
    'uw':'UW','er':'ER','ay':'AY','aw':'AW','oy':'OY',
    'ax':'AH','ix':'IH','rr':'ER','ir':'IY R','ar':'AA R',
    'or':'AO R','ur':'UH R','yu':'Y UW',
    'w':'W','y':'Y','r':'R','l':'L',
    'lx':'L','rx':'R','yx':'Y','hx':'HH','h':'HH',
    'el':'L','en':'N',
    'm':'M','n':'N','nx':'NG',
    'p':'P','b':'B','t':'T','d':'D','k':'K','g':'G',
    'tx':'T','dx':'D','df':'T',
    'q':'_',
    'f':'F','v':'V','th':'TH','dh':'DH',
    's':'S','z':'Z','sh':'SH','zh':'ZH',
    'ch':'CH','jh':'JH',
    'dz':'D Z',
  };

  for (const [k, v] of Object.entries(DT_TO_ARPABET))
    DT_TO_ARPABET[k.toUpperCase()] = v;

  const ALL_DT_PHONES = [...new Set(Object.keys(DT_TO_ARPABET).map(k => k.toLowerCase()))]
    .sort((a, b) => b.length - a.length);

  const VOWELS = new Set([
    'IY','IH','EY','EH','AE','AA','AH','AO','OW',
    'UH','UW','ER','AY','AW','OY',
  ]);

  const INHERENT_DUR = {
    'IY':170,'IH':160,'EY':200,'EH':160,'AE':230,
    'AA':240,'AY':250,'AW':250,'AH':160,'AO':240,
    'OW':220,'OY':220,'UH':170,'UW':210,'ER':250,
    'W':60,'Y':75,'R':65,'L':75,'HH':70,
    'M':70,'N':65,'NG':80,
    'F':100,'V':70,'TH':100,'DH':60,
    'S':115,'Z':75,'SH':115,'ZH':70,
    'P':85,'B':80,'T':85,'D':80,
    'K':90,'G':90,'CH':160,'JH':100,'_':0,
  };

  const SPECIAL_INHDR = {
    'ax':120,'ix':120,'rr':180,'ir':230,'ar':250,
    'or':250,'ur':230,'yu':230,'el':160,'en':170,
    'lx':100,'rx':120,'dx':30,'df':30,'tx':85,'q':0,'dz':115,
  };

  function inherentDur(dtPhone) {
    const lo = dtPhone.toLowerCase();
    if (Object.prototype.hasOwnProperty.call(SPECIAL_INHDR, lo)) return SPECIAL_INHDR[lo];
    const arp = DT_TO_ARPABET[lo] || '';
    const primary = arp.split(' ')[0];
    return INHERENT_DUR[primary] || 80;
  }

  const NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'];

  function midiToNote(midi) {
    if (midi < 0) return 'C-1';
    return NOTE_NAMES[midi % 12] + (Math.floor(midi / 12) - 1);
  }

  function noteToMidi(note) {
    const noteMap = {'C':0,'D':2,'E':4,'F':5,'G':7,'A':9,'B':11};
    const m = note.match(/^([A-G])([#b]?)(-?\d+)$/);
    if (!m) return null;
    const [, letter, acc, octStr] = m;
    let semi = noteMap[letter];
    if (acc === '#') semi++;
    else if (acc === 'b') semi--;
    return (parseInt(octStr) + 1) * 12 + semi;
  }

  function applyTempo(text, factor) {
    return text
      .replace(/r(\d+)/g, (_, n) => 'r' + Math.max(1, Math.round(parseInt(n) * factor)))
      .replace(/p(\d+)/g, (_, n) => 'p' + Math.max(1, Math.round(parseInt(n) * factor)));
  }

  function applyPitch(text, semitones) {
    return text.replace(/b([A-G][#b]?-?\d+)/g, (_, noteName) => {
      const midi = noteToMidi(noteName);
      return midi === null ? 'b' + noteName : 'b' + midiToNote(midi + semitones);
    });
  }

  const STOP_CHARS = new Set([' ', '\t', '<', '_', '[']);

  function convert(content) {
    const outputTokens = [];
    let pos = 0;
    const n = content.length;
    const lowerContent = content.toLowerCase();

    while (pos < n) {
      const c = content[pos];
      if (c === ' ' || c === '\t' || c === '\r' || c === '\n') { pos++; continue; }

      // [: ...] command block — skip entirely
      if (c === '[' && pos + 1 < n && content[pos + 1] === ':') {
        const end = content.indexOf(']', pos);
        pos = end !== -1 ? end + 1 : n;
        continue;
      }

      // [...] phoneme block
      if (c === '[') {
        const end = content.indexOf(']', pos);
        if (end === -1) { pos++; continue; }
        const phoneText = content.slice(pos + 1, end);
        const lowerPhoneText = phoneText.toLowerCase();
        pos = end + 1;

        let p = 0;
        const pn = phoneText.length;
        while (p < pn) {
          const pc = phoneText[p];
          if (pc === ' ' || pc === '\t') { p++; continue; }

          // silence marker _<dur> or _
          if (pc === '_') {
            p++;
            let dur = null;
            if (p < pn && phoneText[p] === '<') {
              const m = phoneText.slice(p).match(/^<(\d+)(?:,\d+)?>/);
              if (m) { dur = parseInt(m[1]); p += m[0].length; }
            }
            outputTokens.push(dur !== null ? `p${dur}` : ',');
            continue;
          }

          // read a run of phoneme symbols up to whitespace / < / _ / [
          const chunkPhones = [];
          const startP = p;
          while (p < pn && !STOP_CHARS.has(phoneText[p])) {
            let matched = null;
            for (const sym of ALL_DT_PHONES) {
              if (lowerPhoneText.startsWith(sym, p)) { matched = sym; break; }
            }
            if (matched) { chunkPhones.push(matched); p += matched.length; }
            else p++;
          }

          // optional <dur,pitch> tag
          let dur = null, pitch = null;
          if (p < pn && phoneText[p] === '<') {
            const m = phoneText.slice(p).match(/^<(\d+)(?:,(\d+))?>/);
            if (m) {
              dur = parseInt(m[1]);
              if (m[2] !== undefined) pitch = parseInt(m[2]);
              p += m[0].length;
            }
          }

          if (chunkPhones.length === 0) { if (p === startP) p++; continue; }

          // find target: last vowel in chunk, or last phone
          let targetIdx = chunkPhones.length - 1;
          for (let i = chunkPhones.length - 1; i >= 0; i--) {
            const arp = DT_TO_ARPABET[chunkPhones[i]] || '';
            if (arp.split(' ').some(tok => VOWELS.has(tok))) { targetIdx = i; break; }
          }

          for (let i = 0; i < chunkPhones.length; i++) {
            const phone = chunkPhones[i];
            const arp = DT_TO_ARPABET[phone];
            if (arp === undefined) continue;
            const tokens = arp.split(' ');
            const isTarget = (i === targetIdx);

            if (isTarget) {
              if (pitch !== null) outputTokens.push('b' + midiToNote(pitch + 35));
              outputTokens.push('r' + (dur !== null ? dur : inherentDur(phone)));
            } else {
              outputTokens.push('r' + inherentDur(phone));
            }

            outputTokens.push((isTarget || tokens.length > 1) ? '( ' + tokens.join(' ') + ' )' : tokens[0]);
            outputTokens.push('r');
          }
        }
        continue;
      }

      // bare punctuation pauses
      if (c === ',' || c === '.' || c === ';') { outputTokens.push(c); pos++; continue; }

      pos++;
    }

    return outputTokens.join(' ');
  }

  global.Dec2KlattschConverter = { convert, applyTempo, applyPitch };
})(window);
