# Per-note step modes: build status

Spec: SPEC-PERNOTE.md. Each line says what file it is in.

## Done

- Spec written (SPEC-PERNOTE.md).

## Left to build

- core.h: `step_t.nmode`, the NM_* values and helpers; track_t's held-note masks.
- seq.c: per-note playback (carry, glide, tie through one step, accent); gate end keeps held notes; REST and
  CHANCE miss let tied notes ring.
- Every place that makes a step fresh or moves its notes: modes cleared or moved with them.
- bank.c: version 3 records, 960-byte records, 3840-byte chunks, reading 3552-byte chunks, backups both sizes.
- editor.c: STEP_SET / TRACK_STEP 2 more bytes, INFO tag 6E, the step signature.
- params.c / ui.c / ui_input.c / ui_draw.c / ui_graph.c: SEQ > NOTES page, roll drawing.
- web/editor.html: per-note cells in the step table, grid marks.
- tests/pernote_test.c in tests/run_tests.sh.

## Left to test

- Host: playback cases, bank round trips (v1 v2 v3, old chunks), backup both sizes, editor protocol, the NOTES
  page through the UI render lint.
- GitHub build links (RAM of the bigger records and steps).
- On the FM-1: everything in SPEC-PERNOTE.md "Playback", the NOTES page, saving and reloading a pattern with modes,
  an old pattern bank after updating, the web editor's note cells.
