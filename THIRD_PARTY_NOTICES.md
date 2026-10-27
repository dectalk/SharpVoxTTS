# Third-Party Notices

SharpVox includes code and data adapted from third-party projects. The notices
below are reproduced as required by their licenses.

## CMU flite (festvox/flite)

The letter-to-sound fallback in `src/LetterToSoundFlite.cpp`,
`src/LetterToSoundFliteModel.cpp`, `include/LetterToSoundFlite.h` and
`include/LetterToSoundFliteModel.h` is adapted from the CMU flite speech
synthesis system (https://github.com/festvox/flite, tag `v2.2`). Specifically:

- `src/lexicon/cst_lts.c` and `include/cst_lts.h` — the decision-tree
  tree-walking engine (`LetterToSoundFlite.cpp`).
- `lang/cmulex/cmu_lts_model.c` and `lang/cmulex/cmu_lts_model.h` — the compiled
  decision-tree states (`LetterToSoundFliteModel.cpp`).
- `lang/cmulex/cmu_lts_rules.c` — the phone table and per-letter state index
  (`LetterToSoundFliteModel.cpp`).

These files were ported to C++: flite's `cst_val` linked-list output and its
`cst_alloc` / `cst_sprintf` / `cst_streq` / `cons_val` utility layer were
replaced by the standard library, the model is read with portable little-endian
byte accesses instead of a struct `memmove` plus an endianness swap, and the
emitted phone symbols are mapped to this project's `PhonemeDefs` phoneme ids
instead of ARPAbet strings. The numeric model tables are unchanged.

The CMU flite sources used here carry the following notice:

```
                  Language Technologies Institute
                     Carnegie Mellon University
                      Copyright (c) 1999-2017
                        All Rights Reserved.

  Permission is hereby granted, free of charge, to use and distribute
  this software and its documentation without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of this work, and to
  permit persons to whom this work is furnished to do so, subject to
  the following conditions:
   1. The code must retain the above copyright notice, this list of
      conditions and the following disclaimer.
   2. Any modifications must be clearly marked as such.
   3. Original authors' names are not deleted.
   4. The authors' names are not used to endorse or promote products
      derived from this software without specific prior written
      permission.

  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK
  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE
  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
  THIS SOFTWARE.
```
