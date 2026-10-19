# SeattleUser0/skiff

A fork of [illuminus5/skiff](https://github.com/illuminus5/skiff) 1.3.3. Everything else is Skiff as released.

## Changes

### 1.3.3-hold: SLIDE HOLD

In stock Skiff, a TIE step holds every note of the step before it. There was no way to hold two notes while a third
note on the same track came and went. SLIDE didn't do it either: on a POLY track it sent a new note-on for every note
of the next step, which restarted each held note's envelopes from where they were (a soft re-attack).

Now, on a POLY track, a note that a SLIDE step carries into the next step keeps sounding. It gets no new note-on, so
no new attack, pluck or envelope restart. Notes the next step drops are released. Notes it adds are struck.

How to use it:

- Turn SLIDE on for every step in the run except the last one.
- Put the notes you want held in every step of the run.
- Put the moving note only in the steps where it should play.

Example, one track, POLY:

| Step | Notes | SLIDE | What you hear |
| --- | --- | --- | --- |
| 1 | C E G | on | C E G struck |
| 2 | C E | on | C E keep ringing, G released |
| 3 | C E G | off | C E keep ringing, G struck again |

Limits:

- The moving note retriggers only when it comes back after a step without it. To repeat it on back-to-back beats,
  use a faster DIV (1/32) and leave a step without it between hits.
- A held note keeps the velocity it started with.
- MONO, LEGATO and UNISON tracks keep Skiff's own slide. DRUM and DRUM-X always strike.
- If another track steals a held note's voice, the next step strikes it again.

Code: `seq_carries()` and the note-on loop in `seq_step()`, firmware/src/seq.c. Test: tests/slide_hold_test.c.

## Building

GitHub builds the package (.github/workflows/release.yml, using the shared
[fm1-build](https://github.com/zvenson/fm1-build) workflow):

- Push a tag such as `v1.3.3-hold`. The Release for that tag gets the `.fwsc`.
- Or Actions > release > Run workflow. The `.fwsc` is in the run's "packages" artifact.

Skiff's web installer only installs Skiff's own published package, so install this one from a terminal on the Mac:

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py skiff-1.3.3-hold.fwsc
```

Run it from this repository's folder (it needs the `tools/` folder). It takes about two minutes and the FM-1
restarts on Skiff.
