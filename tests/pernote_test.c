/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PERNOTE (fork 1.3.3-notes, SPEC-PERNOTE.md): each note's own mode, NM_ACC / NM_SLD / NM_AS / NM_TIE (core.h), as the
 * sequencer plays it (seq.c seq_step, seq_gate_end, seq_rest), on the real sequencer, voice and UI sources with the
 * stubs of tests/ui_test.c:
 *   TIE        a tied note sounds on through the next step without a new attack, also over a REST, an empty step or
 *              a failed CHANCE; it ends at that step's gate; repeated with TIE it holds on, step after step.
 *   SLD        a sliding note's voice glides to the closest free pitch of the next step (no note-on, same voice, its
 *              pitch moving there); two slide to their nearest; a same pitch carries; with nowhere to go it ends.
 *   ACC        an accented note plays at 127, the others at the step's velocity.
 *   OLD        no modes: as 1.3.3-hold plays it (every note struck, step SLIDE holds same pitches only).
 *   OTHERS     MONO: a note's SLD holds the step out as the step's SLIDE does; a ratchet never ties out; a drum kit
 *              never carries; stopping and a preview (stopped) leave no note held.
 *   CODE       nm_code / nm_uncode / nm_fit / nm_remove.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

enum { C4 = 60, D4 = 62, E4 = 64, F4 = 65, G4 = 67, A4 = 69 };

static track_t *pn_track(uint32_t len, uint32_t mode)  /* track 1 alone, a loop of len empty steps, GATE 64 */
{
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_GLIDE] = 0;
    song.g[G_SWING] = 0;
    return t;
}
static void put(track_t *t, uint32_t i, const uint8_t *notes, uint32_t n, const uint8_t *modes, uint32_t flags)
{
    step_t *s = &t->step[i];
    uint32_t k;
    memset(s, 0, sizeof *s);
    s->time = ST_NOTE;
    s->n = (uint8_t)n;
    s->vel = 80;
    s->flags = (uint8_t)flags;
    for (k = 0; k < n; k++) {
        s->note[k] = notes[k];
        nm_set(s, k, modes ? modes[k] : NM_NONE);
    }
}
static uint32_t slen(const track_t *t, uint32_t idx) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), idx); }
static int vox(const track_t *t, uint32_t note)   /* the voice holding note with its gate, -1 none */
{
    for (int i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return i;
    return -1;
}
static uint32_t blk;                                /* blocks played since the start */
/* play to `q` quarters into step idx (counted from the start); the note-ons on the way */
static uint32_t to_q(track_t *t, uint32_t idx, uint32_t q)
{
    uint32_t target = 0, k, v0 = vage;
    for (k = 0; k < idx; k++)
        target += slen(t, k % (uint32_t)t->p[P_SLEN]);
    target += slen(t, idx % (uint32_t)t->p[P_SLEN]) * q / 4u;
    while (blk * CTL < target) {
        seq_tick(t, CTL);
        blk++;
    }
    return vage - v0;
}
static void go(void) { blk = 0; seq_start(); }

static int tie(void)
{
    int bad = 0, ok;
    track_t *t = pn_track(4, V_POLY);
    int vc, ve;
    uint32_t ac, ae, n;
    put(t, 0, (const uint8_t[]){C4, E4, G4}, 3, (const uint8_t[]){NM_TIE, NM_TIE, NM_NONE}, 0);
    put(t, 1, (const uint8_t[]){G4}, 1, 0, 0);
    go();
    n = to_q(t, 0, 1);
    vc = vox(t, C4); ve = vox(t, E4);
    ac = vc >= 0 ? t->v[vc].age : 0; ae = ve >= 0 ? t->v[ve].age : 0;
    ok = n == 3u && vc >= 0 && ve >= 0 && vox(t, G4) >= 0;
    to_q(t, 0, 3);                                /* past step 1's gate (64 / 128): G ends, C and E held */
    ok &= vox(t, G4) < 0 && vox(t, C4) == vc && vox(t, E4) == ve;
    bad += check("TIE C E (G not): past the gate C and E still sound, G ended", ok);
    n = to_q(t, 1, 1);
    ok = n == 1u && vox(t, G4) >= 0 && vox(t, C4) == vc && vox(t, E4) == ve && t->v[vc].age == ac && t->v[ve].age == ae;
    bad += check("  step 2 (G alone): G struck, C and E on through it, no new attack (same voices, same age)", ok);
    to_q(t, 1, 3);
    bad += check("  past step 2's gate: C and E end (one more step), nothing left", vox(t, C4) < 0 && vox(t, E4) < 0 &&
                 vox(t, G4) < 0);
    /* a tie chain: C TIE, C TIE, C: one note-on, held to the third step's gate */
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_TIE}, 0);
    put(t, 1, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_TIE}, 0);
    put(t, 2, (const uint8_t[]){C4}, 1, 0, 0);
    go();
    n = to_q(t, 0, 1);
    vc = vox(t, C4);
    ac = vc >= 0 ? t->v[vc].age : 0;
    n += to_q(t, 2, 1);
    ok = n == 1u && vox(t, C4) == vc && vc >= 0 && t->v[vc].age == ac;
    to_q(t, 2, 3);
    bad += check("TIE chain C, C, C: one attack, held into the third step, ended at its gate", ok && vox(t, C4) < 0);
    /* over a REST, an empty step, a failed CHANCE */
    for (uint32_t kind = 0; kind < 3u; kind++) {
        t = pn_track(4, V_POLY);
        put(t, 0, (const uint8_t[]){C4, E4}, 2, (const uint8_t[]){NM_TIE, NM_NONE}, 0);
        if (kind == 0u) t->step[1].time = ST_REST;
        if (kind == 2u) { put(t, 1, (const uint8_t[]){A4}, 1, 0, 0); step_set_chance(&t->step[1], 0); }
        go();
        to_q(t, 0, 1);
        vc = vox(t, C4);
        to_q(t, 1, 1);
        ok = vc >= 0 && vox(t, C4) == vc && vox(t, E4) < 0 && vox(t, A4) < 0;
        to_q(t, 1, 3);
        bad += check(kind == 0u ? "TIE over a REST: rings into it, ends at its gate" :
                     kind == 1u ? "TIE over an empty step: the same" : "TIE over a step whose CHANCE failed: the same",
                     ok && vox(t, C4) < 0);
    }
    return bad;
}

static int slide(void)
{
    int bad = 0, ok;
    track_t *t = pn_track(4, V_POLY);
    int vc, vg;
    uint32_t ac, n;
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_SLD}, 0);
    put(t, 1, (const uint8_t[]){E4}, 1, 0, 0);
    go();
    to_q(t, 0, 1);
    vc = vox(t, C4);
    ac = vc >= 0 ? t->v[vc].age : 0;
    to_q(t, 0, 3);
    ok = vox(t, C4) == vc && vc >= 0;            /* held past the gate */
    n = to_q(t, 1, 1);
    ok &= n == 0u && vox(t, E4) == vc && t->v[vc].age == ac && t->v[vc].pitch16 == E4 * 16;
    bad += check("SLD C -> E: held past its gate, no note-on, C's voice now E (pitch E4), same age", ok);
    bad += check("  it glides: the voice's pitch not jumped there (no render in between: still where it was)",
                 t->v[vc].pitch_cur != t->v[vc].pitch16);
    to_q(t, 1, 3);
    bad += check("  E (no mode) ends at its gate", vox(t, E4) < 0);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4, G4}, 2, (const uint8_t[]){NM_SLD, NM_AS}, 0);
    put(t, 1, (const uint8_t[]){A4, D4}, 2, 0, 0);
    go();
    to_q(t, 0, 1);
    vc = vox(t, C4); vg = vox(t, G4);
    n = to_q(t, 1, 1);
    bad += check("SLD C and A+S G -> A D: each to its nearest (C to D, G to A), no note-on",
                 n == 0u && vc >= 0 && vg >= 0 && vox(t, D4) == vc && vox(t, A4) == vg);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4, E4}, 2, (const uint8_t[]){NM_SLD, NM_NONE}, 0);
    put(t, 1, (const uint8_t[]){C4, F4}, 2, 0, 0);
    go();
    to_q(t, 0, 1);
    vc = vox(t, C4);
    ac = vc >= 0 ? t->v[vc].age : 0;
    n = to_q(t, 1, 1);
    bad += check("SLD C, E -> C F: C carries (same pitch), E ended, F struck",
                 n == 1u && vox(t, C4) == vc && vc >= 0 && t->v[vc].age == ac && vox(t, F4) >= 0 && vox(t, E4) < 0);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_SLD}, 0);
    t->step[1].time = ST_REST;
    go();
    to_q(t, 0, 3);
    ok = vox(t, C4) >= 0;
    to_q(t, 1, 1);
    bad += check("SLD into a REST: held to it, ends as it starts", ok && vox(t, C4) < 0);
    return bad;
}

static int accent(void)
{
    track_t *t = pn_track(2, V_POLY);
    int vc, ve;
    put(t, 0, (const uint8_t[]){C4, E4}, 2, (const uint8_t[]){NM_ACC, NM_NONE}, 0);
    go();
    to_q(t, 0, 1);
    vc = vox(t, C4); ve = vox(t, E4);
    return check("ACC on C: C at 127, E at the step's 80", vc >= 0 && ve >= 0 && t->v[vc].vel == 127u &&
                 t->v[ve].vel == 80u);
}

static int old(void)
{
    int bad = 0;
    track_t *t = pn_track(3, V_POLY);
    uint32_t n;
    put(t, 0, (const uint8_t[]){C4, E4, G4}, 3, 0, 0);
    put(t, 1, (const uint8_t[]){C4, E4}, 2, 0, 0);
    go();
    n = to_q(t, 0, 1) + to_q(t, 1, 1);
    to_q(t, 1, 3);
    bad += check("no modes: every note struck (3 + 2), all ended at the gate", n == 5u && !t->seq_n);
    t = pn_track(3, V_POLY);
    put(t, 0, (const uint8_t[]){C4, E4, G4}, 3, 0, SF_SLIDE);
    put(t, 1, (const uint8_t[]){C4, A4}, 2, 0, 0);
    go();
    n = to_q(t, 0, 1) + to_q(t, 1, 1);
    bad += check("  step SLIDE (1.3.3-hold): C carries, E and G end, A struck, nothing glides",
                 n == 4u && vox(t, C4) >= 0 && vox(t, A4) >= 0 && vox(t, E4) < 0 && vox(t, G4) < 0);
    return bad;
}

static int others(void)
{
    int bad = 0;
    track_t *t = pn_track(4, V_MONO);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_SLD}, 0);
    put(t, 1, (const uint8_t[]){E4}, 1, 0, 0);
    go();
    to_q(t, 0, 1);
    bad += check("MONO: a note's SLD holds the step out (as the step's SLIDE)", t->seq_hold == 1u);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_TIE}, 0);
    step_set_ratchet(&t->step[0], 2);
    go();
    to_q(t, 0, 1);
    bad += check("a ratchet step never ties out (no note held past its gate)", !t->seq_keep && !t->seq_hold);
    t = pn_track(4, V_POLY);
    t->engine = t->eng_req = ENGI_DRUM;
    put(t, 0, (const uint8_t[]){36}, 1, (const uint8_t[]){NM_TIE}, 0);
    go();
    to_q(t, 0, 1);
    bad += check("a drum kit: a note's TIE holds nothing", !t->seq_keep && !t->seq_hold);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_TIE}, 0);
    go();
    to_q(t, 0, 3);
    seq_stop();
    for (uint32_t k = 0; k < 16u; k++)
        seq_tick(t, CTL);
    bad += check("stop with a tied note held: nothing left", vox(t, C4) < 0 && !t->seq_n);
    t = pn_track(4, V_POLY);
    put(t, 0, (const uint8_t[]){C4}, 1, (const uint8_t[]){NM_TIE}, 0);
    seq_stop();
    t->seq_idx = t->fire_idx = 0;
    seq_step(t, &t->step[0], div_samples(2), SEQ_ROLLED);   /* as seq_audition plays a preview */
    t->seq_hold = 0;
    t->seq_keep = 0;
    for (uint32_t k = 0; k < 400u; k++)
        seq_tick(t, CTL);
    bad += check("a preview (stopped) of a tied note: let go at its gate", vox(t, C4) < 0);
    return bad;
}

static int code(void)
{
    step_t s;
    uint32_t c, ok = 1;
    for (c = 0; c < 625u; c++) {
        memset(&s, 0, sizeof s);
        s.n = 4;
        ok &= nm_uncode(&s, c) && nm_code(&s) == c;
    }
    ok &= !nm_uncode(&s, 625u);
    memset(&s, 0, sizeof s);
    s.n = 2;
    nm_set(&s, 0, NM_ACC); nm_set(&s, 1, NM_TIE); nm_set(&s, 2, NM_SLD); nm_set(&s, 3, NM_AS);
    nm_fit(&s);
    ok &= nm_get(&s, 0) == NM_ACC && nm_get(&s, 1) == NM_TIE && !nm_get(&s, 2) && !nm_get(&s, 3);
    s.n = 4;
    nm_set(&s, 2, NM_SLD); nm_set(&s, 3, NM_AS);
    nm_remove(&s, 1);
    ok &= nm_get(&s, 0) == NM_ACC && nm_get(&s, 1) == NM_SLD && nm_get(&s, 2) == NM_AS && !nm_get(&s, 3);
    s.nmode[0] = 0x77;                              /* (a value past NM_COUNT reads as none) */
    ok &= nm_get(&s, 0) == NM_NONE;
    return check("CODE: 625 numbers both ways, 625 refused, nm_fit drops notes past n, nm_remove moves the rest", ok);
}


/* SEQ > NOTES (ui_input.c notes_edit): KNOB 2 picks a note of the cursor step, KNOB 3 its pitch (past a pitch the
 * step has), KNOB 4 its mode (- ACC SLD A+S TIE, clamped); an empty step gets a note from KNOB 3; on a drum track
 * the page is not there (as ROLL) */
static void go_notes(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "NOTES"); i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}
static int notes_ui(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t i;
    ui_power_on();
    stop_transport();
    t = TSEL;
    track_defaults_steps(t);
    put(t, 0, (const uint8_t[]){C4, E4, G4}, 3, 0, 0);
    ui.cursor = 0;
    ui.npick = 0;
    go_notes();
    ok = str_eq(cur_page()->title, "NOTES") && notes_page(cur_page()) && cur_page()->graph == GR_ROLL;
    turn(EN_K2, 1);
    ok &= ui.npick == 1u;
    turn(EN_K4, 1);
    ok &= nm_get(&t->step[0], 1) == NM_ACC && !nm_get(&t->step[0], 0);
    for (i = 0; i < 6u; i++)
        turn(EN_K4, 1);
    ok &= nm_get(&t->step[0], 1) == NM_TIE;      /* (clamped at TIE) */
    turn(EN_K4, -1);
    ok &= nm_get(&t->step[0], 1) == NM_AS;
    bad += check("NOTES: KNOB 2 picks note 2, KNOB 4 its mode - ACC .. TIE (clamped), A+S back", ok);
    turn(EN_K3, 2);
    ok = t->step[0].note[1] == F4 + 1u;          /* E 64 -> F 65 -> F# 66 */
    turn(EN_K3, 1);                               /* 66 -> 67 is G (note 3): passed over, 68 */
    ok &= t->step[0].note[1] == 68u && t->step[0].note[2] == G4 && nm_get(&t->step[0], 1) == NM_AS;
    bad += check("  KNOB 3 moves note 2's pitch, past the step's G, its mode with it", ok);
    turn(EN_K2, 5);
    ok = ui.npick == 2u;
    turn(EN_K2, -9);
    ok &= ui.npick == 0u;
    bad += check("  KNOB 2 stays among the step's notes (1..3)", ok);
    turn(EN_K1, 1);                               /* the cursor to step 2, empty */
    ok = ui.cursor == 1u && !t->step[1].n;
    turn(EN_K4, 1);
    ok &= !nm_any(&t->step[1]);
    turn(EN_K3, 1);
    ok &= t->step[1].n == 1u && t->step[1].time == ST_NOTE && !nm_any(&t->step[1]);
    bad += check("  an empty step: KNOB 4 nothing, KNOB 3 a note (no mode)", ok);
    ui_power_on();
    stop_transport();
    go_notes();
    i = ui.page;
    ok = page_visible(i);
    TSEL->engine = TSEL->eng_req = ENGI_DRUM;
    ok &= !page_visible(i);
    bad += check("  on a synth track the page is there, on a drum track not (as ROLL)", ok);
    return bad;
}

int main(void)
{
    int bad = tie() + slide() + accent() + old() + others() + code() + notes_ui();
    printf("%s\n", bad ? "PERNOTE TEST FAILED" : "pernote tests passed");
    return bad != 0;
}
