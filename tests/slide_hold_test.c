/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLIDE HOLD (fork of Skiff 1.3.3, seq.c seq_carries): on a POLY track, a SLIDE step's notes that the next step plays
 * again keep sounding, with no new note-on; the notes it drops are released; the notes it adds are struck. On the
 * real sequencer and voice sources, with the stubs of tests/ui_test.c:
 *   HOLD       SLIDE C E G, SLIDE C E, C E G: 3 + 0 + 1 note-ons; C and E keep their voices (same age) through all
 *              three steps; G is released on the second step and struck again on the third.
 *   LOOP       the third step has no SLIDE: the loop's next pass starts fresh (3 note-ons again), nothing hangs.
 *   NO SLIDE   the same steps without SLIDE strike every note of every step (3 + 2 + 3), as before.
 *   STOLEN     a carried note whose voice was taken in between is struck again, not left silent.
 *   MONO       a MONO track's slide is not touched (voice.c mono_play): no carry check.
 *   DRUM       an engine of hits always strikes.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

enum { C4 = 60, E4 = 64, G4 = 67 };

/* track 1 alone, a three-step loop: SLIDE (or not) C E G, SLIDE (or not) C E, then C E G; GATE 64, no swing */
static track_t *three_steps(uint32_t slide, uint32_t mode)
{
    track_t *t;
    uint8_t f = slide ? SF_SLIDE : 0;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 3;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    t->p[P_VOICE] = (int16_t)mode;
    t->p[P_GLIDE] = 0;
    song.g[G_SWING] = 0;
    t->step[0] = (step_t){{C4, E4, G4}, 3, ST_NOTE, f, 100};
    t->step[1] = (step_t){{C4, E4}, 2, ST_NOTE, f, 100};
    t->step[2] = (step_t){{C4, E4, G4}, 3, ST_NOTE, 0, 100};
    seq_start();
    return t;
}
static uint32_t step_len(const track_t *t, uint32_t idx) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), idx); }

/* the voice of track t holding `note` with its gate (-1: none) */
static int held_voice(const track_t *t, uint32_t note)
{
    for (int i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note)
            return i;
    return -1;
}

/* play to the middle of step `idx` (steps counted from the start, whole blocks); return the note-ons since `from` */
static uint32_t to_mid(track_t *t, uint32_t idx, uint32_t *at_block)
{
    uint32_t target = 0, k, v0 = vage;
    for (k = 0; k < idx; k++)
        target += step_len(t, k % 3u);
    target += step_len(t, idx % 3u) / 4u;          /* a quarter in: inside the 64 / 128 gate */
    while (*at_block * CTL < target) {
        seq_tick(t, CTL);
        (*at_block)++;
    }
    return vage - v0;
}

static int sh_hold(void)
{
    int bad = 0;
    uint32_t b = 0, n0, n1, n2, n3;
    track_t *t = three_steps(1, V_POLY);
    int vc, ve, vg;
    uint32_t age_c, age_e;
    n0 = to_mid(t, 0, &b);
    vc = held_voice(t, C4), ve = held_voice(t, E4), vg = held_voice(t, G4);
    bad += check("HOLD step 1 (SLIDE C E G): 3 note-ons, all three held", n0 == 3u && vc >= 0 && ve >= 0 && vg >= 0);
    age_c = vc >= 0 ? t->v[vc].age : 0;
    age_e = ve >= 0 ? t->v[ve].age : 0;
    n1 = to_mid(t, 1, &b);
    bad += check("HOLD step 2 (SLIDE C E): no note-on, G released", n1 == 0u && held_voice(t, G4) < 0);
    bad += check("  C and E still on their voices, not restarted (same age)",
                 held_voice(t, C4) == vc && held_voice(t, E4) == ve && t->v[vc].age == age_c && t->v[ve].age == age_e);
    n2 = to_mid(t, 2, &b);
    bad += check("HOLD step 3 (C E G): 1 note-on, G struck again", n2 == 1u && held_voice(t, G4) >= 0);
    bad += check("  C and E carried through all three steps",
                 held_voice(t, C4) == vc && held_voice(t, E4) == ve && t->v[vc].age == age_c && t->v[ve].age == age_e);
    n3 = to_mid(t, 3, &b);
    bad += check("LOOP next pass (no SLIDE on step 3): fresh, 3 note-ons", n3 == 3u);
    {                                               /* stop: nothing left with its gate */
        int any = 0;
        seq_stop();
        for (uint32_t k = 0; k < 64u; k++)
            seq_tick(t, CTL);
        for (int i = 0; i < NVOICE; i++)
            any |= t->v[i].gate;
        bad += check("LOOP stop: no voice left gated", !any && t->seq_n == 0u);
    }
    return bad;
}

static int no_slide(void)
{
    uint32_t b = 0, n = 0;
    track_t *t = three_steps(0, V_POLY);
    n += to_mid(t, 0, &b);
    n += to_mid(t, 1, &b);
    n += to_mid(t, 2, &b);
    return check("NO SLIDE: every note of every step struck (3 + 2 + 3)", n == 8u);
}

static int stolen(void)
{
    uint32_t b = 0, n1;
    track_t *t = three_steps(1, V_POLY);
    int vc;
    to_mid(t, 0, &b);
    vc = held_voice(t, C4);
    if (vc >= 0)
        voice_kill(&t->v[vc]);                      /* as a steal from another track would */
    for (uint32_t k = 0; k < 8u; k++)
        seq_tick(t, CTL), b++;
    n1 = to_mid(t, 1, &b);
    return check("STOLEN: a carried note whose voice was taken is struck again", n1 == 1u && held_voice(t, C4) >= 0);
}

static int mono(void)
{
    track_t *t = three_steps(1, V_MONO);
    uint32_t b = 0, c = 0;
    to_mid(t, 0, &b);
    for (uint32_t i = 0; i < t->seq_n; i++)
        c |= (uint32_t)seq_carries(t, t->seq_notes[i]);
    return check("MONO: no carry (mono_play keeps its own slide)", c == 0u);
}

static int drum(void)
{
    track_t *t = three_steps(1, V_POLY);
    uint32_t b = 0, c = 0;
    t->engine = t->eng_req = ENGI_DRUM;
    to_mid(t, 0, &b);
    for (uint32_t i = 0; i < t->seq_n; i++)
        c |= (uint32_t)seq_carries(t, t->seq_notes[i]);
    return check("DRUM: an engine of hits always strikes (no carry)", c == 0u);
}

int main(void)
{
    int bad = sh_hold() + no_slide() + stolen() + mono() + drum();
    printf("%s\n", bad ? "SLIDE HOLD TEST FAILED" : "slide hold tests passed");
    return bad != 0;
}
