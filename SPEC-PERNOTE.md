# Per-note step modes: specification

Fork: SeattleUser0/skiff, on Skiff 1.3.3. Release name: 1.3.3-notes.
Rev 1, 2026-10-19: first version.
Rev 2, 2026-10-19: after an independent review. A tie into a ratchet or FLAM step lasts to that step's gate; a tie
rung through a step is never held on by that step's SLIDE; nothing held over ends after a glide lands on its pitch;
chunks without note modes are written in the old size; the web editor's fallback is the old text box.

## What it is

Each note in a step (up to 4) gets its own mode:

| Mode | Shown | What it does |
| --- | --- | --- |
| none | `-` | Plays as the step says. |
| accent | `ACC` | This note plays at velocity 127. |
| slide | `SLD` | This note's voice carries into the next step and glides to a note there. |
| accent + slide | `A+S` | Both. |
| tie | `TIE` | This note keeps sounding through the next step. |

The step's own TIME (NOTE, TIE, REST) and FLAG (-, ACC, SLD, A+S) stay as they are and still apply to the whole
step. A note's mode adds to the step's: a step ACC accents every note, a note ACC accents that note.

## Playback (POLY tracks)

A note "carries" when it goes into the next step without a new attack: no new note-on, no envelope restart, no new
pluck or strike.

1. **Same pitch carries.** If a note held into the next step (by its own TIE or SLD, by the step's SLIDE, or by the
   SLIDE HOLD rule of 1.3.3-hold) finds the same pitch in the next step, it carries. That next-step note's own mode
   then decides what happens after it.
2. **Slide glides.** A note with SLD (or A+S) whose pitch is not in the next step glides to the closest pitch of the
   next step that is not already carried or taken by another slide. Slides are matched in the order of the notes in
   the step; a tie in distance goes to the lower pitch. The glide uses the track's GLIDE time (GLIDE 0: Skiff's
   fixed slide, about 45 ms). The voice keeps its envelope; the note it lands on takes over its mode. A slide with no
   pitch left to go to ends at the start of the next step.
3. **Tie holds one more step.** A note with TIE whose pitch is not in the next step keeps sounding through the next
   step anyway and ends at that step's gate. This also holds over a REST, an empty step or a step whose CHANCE
   failed. To hold it longer, put the same pitch in the next step with TIE again, or make the next step a TIE step.
4. **Step SLIDE stays as in 1.3.3-hold.** A step with SLIDE holds its notes into the next step; same pitches carry,
   the rest end. It never glides on a POLY track.
5. **Everything else** plays as in Skiff 1.3.3: notes not held end at their gate, new notes strike.
6. **Ratchets and FLAM** never tie or slide out (as Skiff already does for the step SLIDE). A ratchet's repeats strike
   every note again. A note tied into a ratchet or FLAM step rings through all its parts and ends at the last part's
   gate.
6a. **A tie is spent after its one extra step.** A step SLIDE on the step it rings through does not carry it further.
7. **A held note whose voice was stolen** by another track is struck again in the next step, not left silent.

## Other voice modes and engines

- **MONO, LEGATO, UNISON:** a note's SLD or TIE counts as the step's SLIDE, and its ACC accents it. Skiff's own mono
  slide and legato then apply.
- **DRUM and DRUM-X** (engines of hits): a note's ACC accents it; TIE and SLD do nothing.
- **Engines that map keys themselves** (SLICE): SLD carries a same-pitch note but never glides.

## Device UI: SEQ > NOTES

A new SEQ page after ROLL, on synth tracks (not drum tracks, as ROLL). It shows the piano roll.

| Knob | Label | What it does |
| --- | --- | --- |
| 1 | STEP | Moves the step cursor, as on ROLL. |
| 2 | NOTE | Picks a note of the cursor step: `1/3` is the first of three. `--` on an empty step. |
| 3 | PITCH | Changes the picked note's pitch, in semitones. |
| 4 | MODE | The picked note's mode: `-`, `ACC`, `SLD`, `A+S`, `TIE`. |

- The notes are numbered as stored: in the order they were entered.
- Keys and buttons do what they do on ROLL (entering notes, EDIT clearing a step, holding a step for locks).
- The picked note is outlined on the roll.
- On the roll, a note with SLD draws a line to the note it slides to, TIE draws its bar on through the next step,
  ACC draws the note bright. The step FLAG drawing is unchanged.

## Storage

- RAM: `step_t` gains `nmode[2]`, a nibble a note (note k: byte k / 2, bits 4 (k % 2) and up), values 0..4.
- Pattern bank (flash): a pattern record grows from 888 to 960 bytes, a chunk of 4 from 3552 to 3840 bytes (the
  most a chunk can hold). A record keeps the version it had (1 or 2) unless a step has a note mode; then it is
  version 3. Version 3 adds, after byte 888, a 72-byte plane: each step's four modes as one number 0..624
  (m0 + 5 m1 + 25 m2 + 125 m3), its low 9 bits in the plane (64 x 9 bits, low bit first) and its bit 9 in bit 6 of
  the step's meta byte. To free that bit, a version 3 meta byte stores n and TIME together (n x 3 + TIME, 0..14) in
  bits 0..3 and the step flags in bits 4..5; bit 7 is FLAM as before. Version 3 motion is version 2's.
- A chunk written by any earlier build (3552 bytes) still loads: its patterns read as before, with no note modes.
- A chunk is written in the new size (3840) only when one of its 4 patterns holds a note mode. Otherwise it is
  written at 3552, exactly as earlier builds write it.
- Backups: a chunk object is 3840 bytes; a backup from an earlier build (3552-byte chunks) restores.
- Projects (SKF3) hold no steps (the pool does) and do not change. User presets hold no pattern and do not change.
- Going back to stock Skiff: chunks without note modes read as always. A chunk with note modes (3840) is refused by
  stock Skiff, so all 4 patterns of that chunk (patterns 1-4, 5-8, and so on) are missing there, and stock Skiff may
  reuse its sector. Clear the note modes (or back up with this build's editor) before going back.

## Editor (SysEx)

- INFO adds the tag `6E 01 05` at the end: per-note modes, 5 values.
- STEP_SET and TRACK_STEP take 2 more bytes after NUDGE: modes 0 and 1 (`m0 | m1 << 3`), modes 2 and 3
  (`m2 | m3 << 3`). STEP_SET with 19 bytes, TRACK_STEP with 20. The reply (STEP_GET, STEP_SET, TRACK_STEP) always
  ends with these 2 bytes.
- A STEP_SET without them keeps the step's note modes.
- The live-sync signature of a step includes its note modes.

## Web editor

- The step table's NOTES text box is replaced by up to 4 note cells. Each cell: the pitch (typed, as before:
  `C4`, `F#3`) and a mode menu (`-`, `ACC`, `SLD`, `A+S`, `TIE`). An empty cell after the last note adds one;
  clearing a cell's pitch removes that note.
- The step's own ACC and SLIDE boxes stay.
- The step grid marks a note's mode after it: `>` ACC, `~` SLD, `>~` A+S, `_` TIE.
- An editor connected to firmware without the `6E` tag shows the old NOTES text box.
