# Per-note step modes: build status

Spec: SPEC-PERNOTE.md. Rev 2, 2026-10-19 (after an independent review of rev 1).

## Done

- core.h: `step_t.nmode[2]`, NM_NONE / NM_ACC / NM_SLD / NM_AS / NM_TIE and helpers (nm_get, nm_set, nm_fit,
  nm_remove, nm_code, nm_uncode); track_t's seq_keep / seq_sld / seq_tie masks; seq_notes 8 + NLANE.
- seq.c: POLY playback (same pitch carries, SLD glides to the nearest free pitch, TIE rings through one more step,
  ACC 127); seq_gate_end keeps held notes; seq_rest lets tied notes ring over a REST, an empty step or a failed
  CHANCE; MONO / LEGATO / UNISON fold a note's SLD / TIE into the step's hold; ratchets and FLAM never hold out; a
  preview never leaves a note held.
- Steps made fresh or with notes moved: modes cleared or moved (ui.c step_clear, grid_own; eng_drum.c step_to_grid;
  seq.c rec_put and the recorded TIE; ui_input.c chord entry and ROLL's NOTE knob; ui_tools.c euclid copy; project.c
  proj_steps).
- bank.c: 960-byte records, 3840-byte chunks, version 3 records only when a step has a mode (bk_put10 / bk_get10),
  chunks of earlier builds (3552) read and written in the new size when next saved, backups of both sizes.
- editor.c / editor_backup.c: STEP_SET 19 bytes, TRACK_STEP 20, the reply's 2 mode bytes, INFO 6E 01 05, the live
  sync signature; a 3552-byte bank chunk restores.
- SEQ > NOTES page (params.c, ui.c notes_page / npick_of, ui_input.c notes_edit, ui_draw.c columns, ui_graph.c roll:
  TIE line, SLD line, ACC bright, the picked note bright and framed).
- web/editor.html: INFO 6E, the 2 bytes both ways, the mock device, note cells (pitch + mode) in the step table,
  grid marks (> ~ >~ _), help text in English and Japanese; a stale reply never overwrites a newer edit or the cell
  being typed into.
- Tests: tests/pernote_test.c (playback, NOTES page), tests/bank_test.c pernote_bank, tests/editor_test.c, the
  ui_render scenes notes_modes / notes_empty, web/test_web.mjs editorNoteModes; existing tests updated for the
  bigger records, the 2 reply bytes, INFO's last tag and the SEQ page order.
- Rev 2 (review fixes): seq_thru mask (a tie rung through: kept through a ratchet's or FLAM's parts, never held on by a
  step SLIDE); POLY playback releases notes not held before any glide; bk_write writes a chunk without note modes at
  3552 (bk_squeeze); tests for each in tests/pernote_test.c review() and tests/bank_test.c pernote_bank().
- Rev 3: bank.c without memmove (the firmware's own libc has none: the first GitHub build failed on it).
- .github/workflows/release.yml: version 1.3.3-notes; a GitHub Pages job with the fork's installer and editor
  (tools/fork_site.py).

## Left to build

- Nothing for this revision.

## Left to test

- GitHub build: it links (the bigger records and steps fit in RAM), and its tests.
- On the FM-1:
  - TIE: C E G with C and E tied, then G alone: C and E ring on, no re-attack. Over a REST too.
  - SLD: C slides to E in the next step, at GLIDE 0 and at a higher GLIDE.
  - ACC on one note of a chord.
  - SEQ > NOTES: the knobs, the roll marks, the picked note.
  - Save a pattern with modes, power off and on: the modes are back.
  - After updating from 1.3.3 or 1.3.3-hold: the old patterns all load.
  - A backup from before, restored.
- Web editor on the Mac (Chrome) and the iPad (MIDIWeb): the note cells, writing modes, reading them back.
