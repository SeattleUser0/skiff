/* SPDX-License-Identifier: GPL-3.0-only */
/* Skiff's pattern pool (firmware/src/bank.c) against simulated NOR flash, with storage.c, user presets and projects
 * as the firmware has them:
 *   a pattern kept exactly as played (every step field, NUDGE, LEN DIV SWING GATE DIR, automation and locks) and
 *   loaded back whole, over a power cycle; the pool's sectors inside the store, apart from every other object and from
 *   the package's BTIF; a write torn at any page leaves the pattern as it was; a damaged newest copy falls back to the
 *   copy before; writes go round the spare sectors; the sequence wraps; nothing is written while the transport runs.
 *   (phase 5) Each track's list names pool patterns; what a track has is kept as the pattern it is on (waiting, then
 *   written at the quiet stop), a new one gets the first free id and the entry it is on (or the first empty one);
 *   PATTERNS FULL; an erased entry; a pattern without room for its automation keeps its own. Live switching: an entry
 *   staged while playing comes in at its launch point (LOOP BAR BEAT NOW) whole, with its automation, what went out
 *   kept; cancelled, replaced, at once when stopped first, never over a project loaded; a switch only with room to
 *   keep what goes out. Scenes.
 * The SAVE > PATTERNS page: tests/ui_test.c test_bank. Built and run by tests/run_tests.sh. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define FELUCCA_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
#include "../firmware/src/menu_items.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int fail_after = -1, erase_fail;
static uint32_t erases[256];                      /* per sector (offset >> 12) */
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    erases[(off >> 12) & 255u]++;
    if (erase_fail)
        return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)   /* (fail_after: the power goes after that many) */
{
    const uint8_t *s = src;
    uint32_t i;
    if (fail_after == 0)
        return -9;
    if (fail_after > 0)
        fail_after--;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static void fm1_wdt_feed(void) {}
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#include "../firmware/src/bank.c"

static int check(const char *what, int ok)
{
    printf("bank: %-72s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static void fresh(void)                           /* a new unit: flash erased, the tracks at power-on, the lists empty */
{
    memset(nor, 0xFF, sizeof nor);
    memset(erases, 0, sizeof erases);
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&ui, 0, sizeof ui);
    memset(&motion, 0, sizeof motion);
    memset(proj_slot, 0, sizeof proj_slot);
    host_tracks_init();
    undo_clear();
    flash_ok = 1;
    fail_after = -1;
    erase_fail = 0;
    bank_boot();
    lists_clear();
    motion_lost = 0;
    memset(launch_q, 0, sizeof launch_q);
    memset((void *)mute_arm, 0, sizeof mute_arm);
    transport_req = 0;
}
static void power_cycle(void) { bank_boot(); }    /* (the RAM gone: the pool read from flash again) */
static uint32_t erases_total(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < 256u; i++)
        n += erases[i];
    return n;
}

/* track k: a pattern using every field a step has (seed: another one) and a full share of motion */
static void rich(uint32_t k, uint32_t seed)
{
    track_t *t = &trk[k];
    uint32_t i, j;
    motion_clear(t);
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        uint32_t r = (i * 7u + seed * 13u) % 11u;
        memset(s, 0, sizeof *s);
        s->time = (uint8_t)(r == 9u ? ST_TIE : r == 10u ? ST_REST : ST_NOTE);
        if (s->time == ST_NOTE) {
            s->n = (uint8_t)(1u + (i + seed) % 4u);
            for (j = 0; j < s->n; j++)
                s->note[j] = (uint8_t)(24u + (i * 5u + j * 7u + seed) % 100u);
            s->vel = (uint8_t)(1u + (i * 37u + seed) % 127u);
            s->flags = (uint8_t)((i % 3u == 0u ? SF_ACCENT : 0u) | (i % 5u == 0u ? SF_SLIDE : 0u));
            s->hit = (uint8_t)(i % 6u == 0u ? 0x81u : i % 6u == 1u ? 0x24u : 0u);
            s->acc = (uint8_t)(s->hit & 0x80u);
        }
        step_set_chance(s, (i * 11u + seed) % 101u);
        if (i % 7u == 3u)
            step_set_rflam(s, 5);                     /* FLAM */
        else
            step_set_ratchet(s, 1u + (i + seed) % 4u);
        s->cond = (uint8_t)((i + seed) % TC_COUNT);
        s->nudge = (int8_t)((int32_t)((i * 13u + seed) % 64u) - 32);
    }
    t->p[P_SLEN] = (int16_t)(37 + (int32_t)seed % 20);
    t->p[P_SDIV] = 3;
    t->p[P_SSWING] = 61;
    t->p[P_SGATE] = 90;
    t->p[P_DIR] = DIR_PING;
    for (i = 0; i < 16u; i++) {                   /* 16 automation events, 16 locks (inside each value's range) */
        const param_desc_t *d = param_desc_of(eng_idx(t->eng_req), P_E0 + i % 8u);
        int32_t span = d->max - d->min + 1;
        motion_set_event(t, (i * 4u + seed) % NSTEP, P_REV, (int16_t)(i * 7u + seed % 9u));
        motion_set_lock(t, (i * 4u + 1u + seed) % NSTEP, P_E0 + i % 8u,
                        (int16_t)(d->min + (int32_t)((i * 5u + seed) % (uint32_t)(span > 64 ? 64 : span))));
    }
}
typedef struct { step_t step[NSTEP]; int16_t p[5]; motion_store_t m; } snap_t;
static void snap(snap_t *s, uint32_t k)           /* what bank.c keeps of track k */
{
    memset(s, 0, sizeof *s);
    memcpy(s->step, trk[k].step, sizeof s->step);
    for (uint32_t i = 0; i < 5u; i++)
        s->p[i] = trk[k].p[BK_TIME[i]];
    motion_snapshot_track(&trk[k], &s->m);
}
static int same(const snap_t *a, const snap_t *b) { return !memcmp(a, b, sizeof *a); }
static void wipe(uint32_t k)                      /* the track emptied: steps, timing, motion */
{
    track_t *t = &trk[k];
    motion_clear(t);
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->p[P_SDIV] = 2;
    t->p[P_SSWING] = 0;
    t->p[P_SGATE] = 64;
    t->p[P_DIR] = DIR_FWD;
}
static void detach(uint32_t k) { trk_pid[k] = 0; wipe(k); }   /* emptied, on no pattern: nothing of it kept */

static int layout(void)
{
    int bad = 0, ok = 1;
    uint32_t i, o, c;
    for (i = 0; i < BK_NSECT; i++) {
        uint32_t s = bk_sector(i);
        ok &= (s >= 0xE7000u && s + 0x1000u <= 0xE9000u) || (s >= 0xEA000u && s + 0x1000u <= 0xFC000u);
        for (o = 0; o < OBJ_COUNT; o++)           /* apart from every storage.c object's sectors */
            for (c = 0; c < 2u; c++)
                ok &= st_sector(o, c) != s;
        ok &= s != 0xE9000u;
        for (o = 0; o < i; o++)
            ok &= bk_sector(o) != s;
    }
    bad += check("20 sectors: the VM area's last two, the package's USR region; not BTIF, nor any other object's", ok);
    bad += check("a pattern 960 bytes (steps, NUDGE, timing, 64 motion records, (PERNOTE) the note-mode plane), 4 a "
                 "sector, 16 chunks + 4 spare", BK_REC1 == 888u && BK_REC == 960u && BK_LEN <= 3840u && BK_CHUNKS == 16u &&
                 BK_NSECT == 20u);
    return bad;
}

static int fidelity(void)
{
    int bad = 0, ok;
    uint32_t e0;
    snap_t a, b, c;
    static uint8_t r[BK_REC], r0[BK_REC];
    static step_t st[NSTEP];
    static motion_store_t ms;
    int16_t v[5];
    fresh();
    rich(1, 3);
    snap(&a, 1);
    ok = bank_put(1, 5, 0) == 0 && bank_used(1, 5) && pool_len(ent_pat(1, 5)) == (uint32_t)trk[1].p[P_SLEN] && !bank_used(1, 4) &&
         !bank_used(0, 5) && a.m.count >= 24u && bank_cur(1) == 5u;
    detach(1);
    ok &= ent_go(1, 5) == 0;
    snap(&b, 1);
    bad += check("track 2's pattern into F and back: every step field, NUDGE, LEN DIV SWING GATE DIR, 16 events, "
                 "16 locks", ok && same(&a, &b));
    rich(2, 7);
    snap(&c, 2);
    ok = bank_put(2, 15, 0) == 0;
    power_cycle();
    detach(1);
    detach(2);
    ok &= bank_used(1, 5) && bank_used(2, 15) && ent_go(2, 15) == 0 && ent_go(1, 5) == 0;
    snap(&b, 1);
    ok &= same(&a, &b);
    snap(&b, 2);
    bad += check("  over a power cycle, another track's P beside it", ok && same(&c, &b));
    ok = ent_pat(1, 5) == 0u && ent_pat(2, 15) == 1u && ent_pat(1, 4) == PAT_NONE && pool_used(0) && pool_used(1) &&
         !pool_used(2) && trk_pat(1) == 0u && trk_pat(2) == 1u && !undo_n;
    bad += check("  (phase 5) the entries name pool ids, the first free ones; each track on its entry's; no undo copy",
                 ok);
    detach(0);
    lists.ent[0][0][0] = 1u;                      /* T1's A names T2's F too: one pattern, two entries */
    ok = ent_go(0, 0) == 2;                       /* T2 and T3 hold 64 records, all there is: AUTOMATION FULL */
    snap(&b, 0);
    bad += check("  a pattern two entries name, onto T1; the motion pool full: the steps load, the automation not",
                 ok && !memcmp(a.step, b.step, sizeof a.step) && !motion_count(&trk[0]) && (motion_lost & 1u));
    e0 = erases_total();
    ok = !pat_read(0, r0) && keep_all() == 0 && erases_total() == e0;   /* (what was loaded, kept: the same, nothing) */
    motion_set_event(&trk[0], 2, P_REV, 9);       /* (refused: the pattern's own did not fit) */
    trk[0].step[1].vel ^= 1u;                     /* an edit: kept, the pattern's own automation with it */
    ok &= !motion_count(&trk[0]) && keep_all() == 0 && !pat_decode(&trk[3], 0, st, v, &ms) &&
          st[1].vel == (a.step[1].vel ^ 1u) && ms.count == a.m.count;
    trk[0].step[1].vel ^= 1u;
    ok &= keep_all() == 0 && !pat_read(0, r) && !memcmp(r, r0, BK_REC);
    bad += check("  .. kept: the same bytes (nothing written); an edit kept, its stored automation with it; none taken",
                 ok);
    detach(2);                                    /* (T3's 32 records gone: room) */
    ok = ent_go(0, 0) == 0;
    snap(&b, 0);
    bad += check("  room again: on T1 whole, the automation placed on T1", ok && !memcmp(a.step, b.step, sizeof a.step) &&
                 !memcmp(a.p, b.p, sizeof a.p) && b.m.count == a.m.count && (b.m.event[0].place >> 6) == 0u &&
                 !(motion_lost & 1u));
    ok = ent_go(3, 0) == 1 && bank_put(1, 5, 1) == 0 && !bank_used(1, 5) && pool_used(0) && bank_used(0, 0) &&
         !trk_pid[1] && seq_is_empty(&trk[1]) && !motion_count(&trk[1]);   /* (T2 was on F: emptied) */
    ok &= bank_put(0, 0, 1) == 0 && !pool_used(0) && !bank_used(0, 0) && seq_is_empty(&trk[0]);
    power_cycle();
    bad += check("  an empty entry loads nothing; ERASE: the entry emptied, its track too; the pattern kept while another "
                 "entry names it, erased with the last", ok && !pool_used(0) && bank_used(2, 15));
    return bad;
}

/* (phase 5) keeping edits: records wait (PEND_N) for a quiet stop, reads see them first, flushes write them */
static int keep(void)
{
    int bad = 0, ok;
    static uint8_t r1[BK_REC], r2[BK_REC], r3[BK_REC], got[BK_REC], zero[BK_REC];
    uint32_t e0, e;
    fresh();
    rich(1, 3);
    ok = bk_pack(r1, &trk[1]);
    rich(2, 7);
    ok &= bk_pack(r2, &trk[2]);
    e0 = erases_total();
    ok &= pat_keep_rec(5, r1) == 0 && pool_used(5) && !bk.len[5] && pend_free() == PEND_N - 1u &&
          !pat_read(5, got) && !memcmp(got, r1, BK_REC) && erases_total() == e0;
    bad += check("keep: a record waits (not written), reads see it, the pool counts it", ok);
    ok = pat_keep_rec(5, r2) == 0 && pend_free() == PEND_N - 1u && !pat_read(5, got) && !memcmp(got, r2, BK_REC);
    bad += check("  the same id again: the waiting copy replaced, no second slot", ok);
    for (ok = 1, e = 6; e < 5u + PEND_N; e++)            /* (ids 5 .. 4 + PEND_N waiting) */
        ok &= pat_keep_rec(e, r1) == 0;
    ok &= !pend_free() && pat_keep_rec(5u + PEND_N, r1) == 1 && !pool_used(5u + PEND_N);
    bad += check("  PEND_N waiting (one a track): one more has no room (1), nothing taken", ok);
    ok = pat_flush_one() == 0 && pend_free() == 3u && bk.len[5] && bk.len[7] && !bk.len[8];   /* (5 6 7: one chunk) */
    ok &= pat_flush_one() == 0 && pend_free() == PEND_N && pat_flush_one() == 0 && erases_total() == e0 + 2u && bk.len[8];
    power_cycle();
    ok &= pool_used(5) && !pat_read(5, got) && !memcmp(got, r2, BK_REC) && pool_used(6) && pend_free() == PEND_N;
    bad += check("  flushed a chunk a call (its waiting patterns in one write), then kept over a power cycle", ok);
    e0 = erases_total();
    ok = pat_keep_rec(5, r2) == 0 && pend_free() == PEND_N && erases_total() == e0;
    bad += check("  the same as stored: nothing waits, nothing written", ok);
    ok = pat_keep_rec(6, zero) == 0 && !pool_used(6) && pat_flush_one() == 0 && !bk.len[6] && !pool_used(6);
    bad += check("  an empty record empties the pattern (at the flush)", ok);
    ok = pool_alloc() == 0u && pat_keep_rec(0, r1) == 0 && pool_alloc() == 1u;
    bad += check("  pool_alloc: the first empty id, a waiting one counts", ok);
    memcpy(r3, r2, BK_REC);
    r3[BK_STEPS] ^= 1u;
    ok = pool_find(r2) == 5u && pool_find(r1) == 0u && pool_find(r3) == NPAT;
    bad += check("  pool_find: the id with the same record (waiting or stored), else none", ok);
    song.playing = 1;
    ok = pat_flush_one() == 2 && pend_free() == PEND_N - 1u;
    song.playing = 0;
    ok &= pat_flush_one() == 0 && pend_free() == PEND_N;
    bad += check("  playing: no flush (it waits); stopped: written", ok);
    return bad;
}

/* ---- (phase 5) the lists: a track's steps kept as the pattern it is on; new ones a new pattern ---- */
static void simple(uint32_t k, uint32_t note, uint32_t len, uint32_t dir)   /* new steps (on no pattern): step i
                                                                             * plays note + i */
{
    track_t *t = &trk[k];
    uint32_t i;
    trk_pid[k] = 0;
    wipe(k);
    for (i = 0; i < len; i++)
        t->step[i] = (step_t){{(uint8_t)(note + i)}, 1, ST_NOTE, 0, 100};
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_DIR] = (int16_t)dir;
}
static int entries(void)
{
    int bad = 0, ok;
    uint32_t i, a, b;
    track_t *t = &trk[0];
    static uint8_t r[BK_REC], rb[BK_REC];
    static step_t st[NSTEP];
    static motion_store_t ms;
    int16_t v[5];
    fresh();
    simple(0, 30, 4, DIR_FWD);                    /* new steps on T1, its list empty */
    ok = !trk_pid[0] && keep_poll() == 1 && trk_pid[0] && ent_pat(0, 0) == trk_pat(0) && !lists.cur[0] &&
         pend_free() == PEND_N && pool_used(trk_pat(0)) && keep_poll() == 0;
    a = trk_pat(0);
    bad += check("entries: new steps kept at the quiet stop (one write a poll): the first free id, named by the entry "
                 "the track is on (A)", ok);
    simple(0, 50, 4, DIR_FWD);                    /* more new steps, A naming a */
    ok = keep_all() == 0 && trk_pat(0) != a && trk_pat(0) != PAT_NONE && ent_pat(0, 1) == trk_pat(0) &&
         lists.cur[0] == 1u && ent_pat(0, 0) == a;
    b = trk_pat(0);
    bad += check("  .. the entry it is on naming another: the first empty one (B), the track on it; A as it was", ok);
    lists.ent[0][1][0] = (uint8_t)(a + 1u);       /* B made to name a (the SONG page's PATTERN), the track still on b */
    t->step[0].note[0] = 52;                      /* an edit */
    ok = keep_all() == 0 && !pat_decode(t, b, st, v, &ms) && st[0].note[0] == 52 && !pat_decode(t, a, st, v, &ms) &&
         st[0].note[0] == 30;
    lists.ent[0][1][0] = (uint8_t)(b + 1u);
    bad += check("  an edit kept as the pattern the track is on (b), whatever its entry names now", ok);
    t->step[1].note[0] = 53;                      /* an edit, then the track to A */
    ok = ent_go(0, 0) == 0 && t->step[0].note[0] == 30 && pend_at(b) < PEND_N;
    ok &= ent_go(0, 1) == 0 && t->step[0].note[0] == 52 && t->step[1].note[0] == 53 && !undo_n;
    bad += check("  leaving a pattern keeps it first (waiting for the quiet stop); back on it: the edit there", ok);
    ok = keep_all() == 0 && pend_free() == PEND_N;
    t->step[2].note[0] = 54;                      /* an edit not kept yet when the power goes */
    power_cycle();
    detach(0);
    ok &= ent_go(0, 1) == 0 && t->step[1].note[0] == 53 && t->step[2].note[0] == 52;
    bad += check("  over a power cycle: what was kept (written stopped), not the edit after it (gone, as before)", ok);

    ok = bk_pack(r, &trk[3]);                     /* (an empty track's pattern) */
    for (i = 0; i < NPAT; i++)                    /* the pool full: every id holds a pattern */
        if (!pool_used(i))
            ok &= pool_put(i, r) == 0;
    simple(0, 70, 4, DIR_FWD);                    /* new steps: no id for them */
    ok &= pool_alloc() == NPAT && keep_all() == 1 && !trk_pid[0] && ent_go(0, 0) == 4 && t->step[0].note[0] == 70 &&
          bank_put(0, 5, 0) == 4 && !bank_used(0, 5) && project_save_as(1, 0) == 1 && str_eq(ui.msg, "PATTERNS FULL");
    song.playing = 1;
    ok &= bank_switch(0, 0) == 4 && bank_next(0) == BK_NONE;
    song.playing = 0;
    ok &= bank_put(0, 1, 0) == 0 && trk_pat(0) == b && !pat_decode(t, b, st, v, &ms) && st[0].note[0] == 70;
    bad += check("  the pool full: new steps get no id (PATTERNS FULL: no switch, no SAVE into an empty entry, no project "
                 "save; the track stays on them); SAVE over a named entry goes", ok);
    ok = bank_put(0, 0, 1) == 0 && !pool_used(a) && pool_alloc() == a;
    bad += check("  an entry erased (nothing else naming its pattern): its id free again", ok);

    ok = bk_pack(rb, t);
    for (i = 0; i < PEND_N - 1u; i++)             /* three waiting (playing: they wait) */
        ok &= pat_keep_rec(10u + i, rb) == 0;
    lists.ent[0][0][0] = (uint8_t)(b + 1u);       /* T1's A and T2's A: b */
    lists.ent[1][0][0] = (uint8_t)(b + 1u);
    song.playing = 1;
    ok &= pend_free() == 1u && bank_switch(0, 0) == 0 && bank_switch(1, 0) == 4 && bank_cancel(0) &&
          bank_switch(1, 0) == 0 && bank_next(1) == 0u;
    bank_cancel(1);
    song.playing = 0;
    bad += check("  playing: a switch staged only with a waiting slot for each track staged (what goes out waits there)",
                 ok && keep_all() == 0 && pend_free() == PEND_N);
    return bad;
}

/* (phase 6) every track staged at once: a waiting slot a track (PEND_N = NTRK); a scene that cannot stage a track (no
 * room to keep what it has) says PATTERNS FULL, its other tracks staged all the same */
static int staged_all(void)
{
    int bad = 0, ok;
    uint32_t k, b;
    static uint8_t r[BK_REC];
    fresh();
    simple(0, 60, 4, DIR_FWD);
    ok = keep_all() == 0 && trk_pid[0] != 0u;     /* T1's steps: a pattern, its entry A */
    b = trk_pid[0] - 1u;
    for (k = 0; k < NTRK; k++)                    /* every track's B: that pattern */
        lists.ent[k][1][0] = (uint8_t)(b + 1u);
    song.playing = 1;
    for (k = 0; k < NTRK; k++)
        ok &= bank_switch(k, 1) == 0 && bank_next(k) == 1u;
    bad += check("every track staged at once: a waiting slot a track (PEND_N = NTRK)", ok && PEND_N == NTRK &&
                 pend_free() == PEND_N);
    for (k = 0; k < NTRK; k++)
        bank_cancel(k);
    lists.scene[0] = 2u;                          /* scene 1: T1 to its B, T2 to its B, the rest left alone */
    lists.scene[0] |= 2u << 5;
    lists.scene[48] = 1u;
    ok = bk_pack(r, &trk[3]);
    for (k = 0; k < PEND_N - 1u; k++)             /* one slot free: T1 stages, T2 has none */
        ok &= pat_keep_rec(20u + k, r) == 0;
    scene_key(0, 0);
    ok &= bank_next(0) == 1u && bank_next(1) == BK_NONE && str_eq(ui.msg2, "PATTERNS FULL");
    bank_cancel(0);
    song.playing = 0;
    bad += check("  a scene with no room for a track: PATTERNS FULL, the tracks with room staged", ok && keep_all() == 0);
    return bad;
}

/* (phase 6) the last track's automation (with six tracks its place 5 << 6 | 63 = 0x17F: past a byte) kept in a pattern
 * and loaded back onto it; a record holding a value no firmware wrote (200: a byte in RAM would wrap it into range):
 * its automation dropped, the steps load */
static int last_track(void)
{
    int bad = 0, ok;
    uint32_t k = NTRK - 1u, i, id, n = 0;
    static uint8_t r[BK_REC];
    static step_t st[NSTEP];
    static motion_store_t ms;
    int16_t v[5];
    fresh();
    simple(k, 40, 64, DIR_FWD);
    ok = motion_put(&trk[k], 63, P_PAN, -64, MOTION_LOCK) == 0 && keep_all() == 0 && trk_pid[k] != 0u;
    id = trk_pid[k] - 1u;
    detach(k);
    ok &= ent_go(k, 0) == 0;
    for (i = 0; i < motion.count; i++)
        n += motion.event[i].place == (k << 6 | 63u) && (motion.event[i].id == P_PAN && motion.event[i].lock) &&
             motion.event[i].value == -64;
    bad += check("the last track's lock on step 64 (its place past a byte with six) kept and loaded back",
                 ok && n == 1u && motion_count(&trk[k]) == 1u);
    ok = !pat_read(id, r) && (r[7] & 127u) == 1u;
    r[BK_MOT + 2u] = 200u;                            /* (its value: 200, 0) */
    r[BK_MOT + 3u] = 0u;
    ok &= pat_keep_rec(id, r) == 0 && !pat_decode(&trk[k], id, st, v, &ms) && !ms.count && st[5].note[0] == 45u;
    bad += check("  a record holding a value outside -64..127: its automation dropped, the steps load", ok);
    /* (Skiff phase 9) the motion's ids as a store of PROJ_NP has them (firmware before reads the record), unless the
     * motion names a parameter added since: then today's */
    fresh();
    simple(k, 40, 64, DIR_FWD);
    ok = motion_put(&trk[k], 3, P_E5, 20, MOTION_LOCK) == 0 && keep_all() == 0 && trk_pid[k] != 0u;
    id = trk_pid[k] - 1u;
    ok &= !pat_read(id, r) && r[1] == PROJ_NP && r[BK_MOT + 1u] == ((P_E5 - PROJ_NX) | MOTION_LOCK) &&
          !pat_decode(&trk[k], id, st, v, &ms) && ms.count == 1u && (ms.event[0].id == P_E5 && ms.event[0].lock);
    ok &= r[0] == BK_VER;
    bad += check("  (phase 9) a lock on E5: the record of 103 parameters (E5 at 100), back at today's P_E5", ok);
    ok = motion_put(&trk[k], 4, P_DTONE, -9, MOTION_LOCK) == 0 && keep_all() == 0 && trk_pid[k] == id + 1u;
    ok &= !pat_read(id, r) && r[1] == P_COUNT && !pat_decode(&trk[k], id, st, v, &ms) && ms.count == 2u;
    for (i = n = 0; i < ms.count; i++)
        n += (ms.event[i].id == P_E5 && ms.event[i].lock) || (ms.event[i].id == P_DTONE && ms.event[i].lock);
    bad += check("  .. and one on DRIVE's TONE (phase 9's): the record of today's ids, both back", ok && n == 2u);
    ok = !pat_read(id, r) && P_E5 > 127u && r[0] == BK_VER2 && (r[BK_MOT] & 64u) &&
         r[BK_MOT + 1u] == ((P_E5 & 127u) | MOTION_LOCK) && !(r[BK_MOT + 4u] & 64u);
    bad += check("  (1.3) E5 is past 127 there: version 2, its id's bit 7 in its step byte's bit 6", ok);
    return bad;
}

static int torn(void)
{
    int bad = 0, ok = 1, n;
    snap_t a, b, c;
    fresh();
    rich(0, 1);
    snap(&a, 0);
    bank_put(0, 2, 0);                            /* the copy before: C, pattern 01 (ids 0..3's chunk) */
    rich(0, 9);
    snap(&c, 0);
    for (n = 0; n <= (int)((BK_LEN + 255u) / 256u); n++) {   /* the power goes after n writes: the payload's pages, the
                                                              * header ((PERNOTE) each page, before the header) */
        rich(0, 9);
        fail_after = n;
        ok &= bank_put(0, 2, 0) != 0;
        fail_after = -1;
        power_cycle();
        detach(0);
        ok &= ent_go(0, 2) == 0;
        snap(&b, 0);
        ok &= same(&a, &b);
    }
    bad += check("a save torn after any write (each page, before the header): C as it was", ok);
    fail_after = -1;
    rich(0, 9);
    ok = bank_put(0, 2, 0) == 0;
    power_cycle();
    detach(0);
    ok &= ent_go(0, 2) == 0;
    snap(&b, 0);
    bad += check("  then a whole one: the new C", ok && same(&c, &b));
    nor[bk_sector(bk.sect[0] - 1u) + 256u + 1000u] ^= 0x10u;   /* the newest copy damaged */
    power_cycle();
    detach(0);
    ok = ent_go(0, 2) == 0;
    snap(&b, 0);
    bad += check("  its newest copy damaged: the copy before (the first C)", ok && same(&a, &b));
    rich(0, 5);                                   /* (changed: else there is nothing to write) */
    erase_fail = 1;
    ok = bank_put(0, 2, 0) != 0;
    erase_fail = 0;
    bad += check("  an erase that fails: not saved, said so", ok && bank_used(0, 2));
    return bad;
}

static int wear_and_wrap(void)
{
    int bad = 0, ok = 1;
    uint32_t i, most = 0;
    snap_t a, b;
    static uint8_t r[BK_REC];
    fresh();
    for (i = 0; i < BK_CHUNKS; i++) {              /* a full bank: every chunk has a copy, 4 sectors spare */
        rich(0, i);
        ok &= bk_pack(r, &trk[0]) && pool_put(i * BK_PER, r) == 0;
    }
    detach(0);
    memset(erases, 0, sizeof erases);
    for (i = 0; i < 40u; i++) {
        rich(3, i);
        ok &= bank_put(3, 9, 0) == 0;
    }
    snap(&a, 3);
    for (i = 0; i < BK_NSECT; i++)
        most = erases[bk_sector(i) >> 12] > most ? erases[bk_sector(i) >> 12] : most;
    power_cycle();
    detach(3);
    ok &= ent_go(3, 9) == 0;
    snap(&b, 3);
    bad += check("a full bank, 40 saves of one entry: round the spare sectors (each erased about 10 times), the last "
                 "kept", ok && same(&a, &b) && most <= 11u && pool_used(0) && pool_used(44) && ent_pat(3, 9) == 1u);
    fresh();
    bk.top = 0xFFFFFFF0u;                         /* the sequence about to wrap */
    for (i = 0; i < 30u; i++) {
        rich(0, i);
        ok &= bank_put(0, 0, 0) == 0;
    }
    snap(&a, 0);
    power_cycle();
    detach(0);
    ok &= ent_go(0, 0) == 0 && bk.top < 0x100u;
    snap(&b, 0);
    bad += check("  the sequence wraps: the newest copy still wins", ok && same(&a, &b));
    return bad;
}

static int playing(void)
{
    int bad = 0;
    uint32_t e;
    fresh();
    rich(0, 4);
    bank_put(0, 1, 0);
    e = erases_total();
    song.playing = 1;
    bad += check("playing: SAVE and ERASE write nothing (an erase mutes)", bank_put(0, 1, 1) == 2 && bank_put(0, 3, 0) == 2 &&
                 e == erases_total() && bank_used(0, 1) && !bank_used(0, 3));
    bad += check("  LOAD stages it (reads only)", bank_switch(0, 1) == 0 && bank_next(0) == 1u && e == erases_total());
    bank_cancel(0);
    song.playing = 0;
    flash_ok = 0;
    bank_boot();
    bad += check("no flash: an empty bank, SAVE says so", !bank_used(0, 1) && bank_put(0, 1, 0) == 3);
    flash_ok = 1;
    return bad;
}

/* ---- live switching (launch.c; bank.c bank_switch, bank_poll): track 0 stepped one step a block ---- */
#define W_ENT 15u                                 /* (W's entry: P) */
static void lv_next(uint32_t n)                   /* track 0 on n steps: each block to the end of its step exactly */
{
    track_t *t = &trk[0];
    while (n--)
        events_block(step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) - t->seq_pos);
}
static int sounds(uint32_t note) { return trk[0].seq_n && trk[0].seq_notes[0] == note; }
static void lv_start(uint32_t mode)               /* stopped; W on track 0 (12 steps, notes 40.., an automation event),
                                                   * saved as P, the track on it; the launch point; PLAY: W's first step
                                                   * sounds */
{
    transport_req = 2;
    events_block(1);
    bank_poll();
    simple(0, 40, 12, DIR_FWD);
    motion_set_event(&trk[0], 3, P_REV, 10);
    bank_put(0, W_ENT, 0);
    ui_launch = (uint8_t)mode;
    transport_req = 1;
    events_block(1);
}
static int live(void)
{
    int bad = 0, ok;
    uint32_t i, bpm, w;
    track_t *t = &trk[0];
    snap_t c, b;
    static project_t pj;
    fresh();
    simple(0, 60, 8, DIR_FWD);                    /* B: 8 steps, notes 60.. */
    bank_put(0, 1, 0);
    simple(0, 80, 8, DIR_REV);                    /* D: 8 steps, notes 80.., backwards */
    bank_put(0, 3, 0);
    simple(0, 80, 8, DIR_FWD);                    /* C: 8 steps, notes 80.., two automation events */
    motion_set_event(t, 0, P_REV, 40);
    motion_set_event(t, 5, P_REV, 90);
    bank_put(0, 2, 0);
    snap(&c, 0);
    motion_set_event(&trk[2], 1, P_REV, 70);      /* (another track's automation: stays as it is) */

    lv_start(LAUNCH_LOOP);
    w = ent_pat(0, W_ENT);
    motion_set_lock(t, 11, P_SGATE, 20);           /* (an edit since: W's last step its own GATE, sounding at the swap) */
    lv_next(5);
    ok = bank_switch(0, 2) == 0 && bank_next(0) == 2u && sounds(45);
    lv_next(6);                                   /* W's last step */
    ok &= sounds(51) && t->p[P_SLEN] == 12 && bank_next(0) == 2u;
    lv_next(1);
    snap(&b, 0);
    ok &= sounds(80) && t->seq_idx == 0 && same(&c, &b) && bank_next(0) == BK_NONE && launch_q[0].done &&
          motion_count(&trk[2]) == 1;
    bad += check("live LOOP: staged mid-pass, the pass plays out, C from its step 1 (steps, LEN, automation)", ok);
    bank_poll();
    ok = !launch_q[0].done && bank_cur(0) == 2u && trk_pat(0) == ent_pat(0, 2) && pend_at(w) < PEND_N && !undo_n;
    transport_req = 2;
    events_block(1);
    ok &= ent_go(0, W_ENT) == 0 && t->p[P_SLEN] == 12 && t->step[0].note[0] == 40 && motion_count(t) == 2 &&
          motion_count(&trk[2]) == 1 && t->p[P_SGATE] == TP[P_SGATE].def;   /* (W's GATE, not its last step's lock) */
    ok &= ent_go(0, 2) == 0;
    snap(&b, 0);
    bad += check("  what went out kept as W's pattern (its edit since: GATE not a lock's), no undo copy; P: W, C: C",
                 ok && same(&c, &b));

    lv_start(LAUNCH_BAR);
    lv_next(5);
    ok = bank_switch(0, 2) == 0;
    lv_next(10);                                  /* the run's step 16: W's 4th, its second pass */
    ok &= sounds(43) && bank_next(0) == 2u;
    lv_next(1);
    ok &= sounds(80) && t->seq_idx == 0;
    bad += check("live BAR: staged in bar 1, C on bar 2's first step, W (12 steps) mid-pass", ok);
    lv_start(LAUNCH_BEAT);
    lv_next(5);
    ok = bank_switch(0, 2) == 0;
    lv_next(2);
    ok &= sounds(47);
    lv_next(1);                                   /* the run's step 9: beat 3 */
    ok &= sounds(80);
    bad += check("live BEAT: on the next beat's step", ok);
    lv_start(LAUNCH_NOW);
    lv_next(5);
    ok = bank_switch(0, 2) == 0 && sounds(45);
    lv_next(1);
    ok &= sounds(86) && t->seq_idx == 6;
    lv_next(2);                                   /* (C's LEN 8: round to its step 1) */
    ok &= sounds(80);
    bad += check("live NOW: the next step, the playhead going on (C's 7th), C's LEN from there", ok);
    lv_start(LAUNCH_LOOP);
    ok = bank_switch(0, 3) == 0;
    lv_next(12);
    ok &= sounds(87) && t->seq_idx == 7;
    lv_next(1);
    ok &= sounds(86);
    bad += check("  a pattern playing backwards (D) starts from its last step", ok);

    for (bpm = 120; bpm < 240u; bpm++) {          /* a tempo where 1/32's steps round shorter than half 1/16's */
        song.g[G_BPM] = (int16_t)bpm;
        if (8u * div_samples(3) < 4u * div_samples(2))
            break;
    }
    lv_start(LAUNCH_BAR);
    t->p[P_SDIV] = 3;                             /* W at 1/32: ahead of the bar lines counted, 4 samples a beat */
    transport_req = 1;
    events_block(1);
    lv_next(40);
    ok = bpm < 240u && bank_switch(0, 2) == 0;
    lv_next(23);
    ok &= sounds(40 + 63 % 12) && bank_next(0) == 2u;
    lv_next(1);                                   /* 1/32 step 65: bar 3, 32 samples before its line */
    ok &= sounds(80);
    bad += check("  BAR: a 1/32 track (its steps rounded short: ahead of the bars counted) comes in on the bar", ok);
    song.g[G_BPM] = 120;

    lv_start(LAUNCH_LOOP);
    lv_next(2);
    ok = bank_switch(0, 1) == 0 && bank_switch(0, 2) == 0 && bank_next(0) == 2u;
    lv_next(10);
    ok &= sounds(80);
    bad += check("  staged again before it comes in: the last one comes in", ok);
    lv_start(LAUNCH_LOOP);
    lv_next(2);
    ok = bank_switch(0, 2) == 0 && bank_cancel(0) && bank_next(0) == BK_NONE && !bank_cancel(0);
    lv_next(12);
    ok &= sounds(42) && !launch_q[0].done;
    bad += check("  cancelled (OCT- in the PERFORM layer): W plays on", ok);
    lv_start(LAUNCH_LOOP);
    lv_next(2);
    ok = bank_switch(0, 2) == 0;
    transport_req = 2;
    events_block(1);
    bank_poll();
    ok &= !song.playing && t->step[0].note[0] == 80 && t->p[P_SLEN] == 8 && bank_next(0) == BK_NONE && bank_cur(0) == 2u;
    ok &= !undo_n && bank_switch(0, W_ENT) == 0 && t->step[0].note[0] == 40;
    bad += check("  stopped before its launch point: it comes in at once (no undo copy: W kept, P brings it back)", ok);

    lv_start(LAUNCH_LOOP);
    for (i = 0; motion.count < MOTION_MAX && i < NSTEP; i++)
        motion_set_event(&trk[1], i, P_REV, 10);
    ok = motion.count == MOTION_MAX && bank_switch(0, 2) == 2;
    lv_next(12);
    ok &= sounds(80) && motion_count(t) == 0 && motion.count == MOTION_MAX - 1u;
    bank_poll();
    motion_set_event(t, 2, P_REV, 33);            /* (refused: C's own did not come in) */
    ok &= (motion_lost & 1u) && !motion_count(t) && trk_rec(0) && (bk_rec[7] & 127u) == 2u;
    bad += check("  no room for its automation: the steps come in, not the automation (AUTOMATION FULL); C kept with "
                 "its own, the track takes none", ok);
    motion_clear(&trk[1]);

    lv_start(LAUNCH_LOOP);
    project_capture(&pj);
    ok = bank_switch(0, 2) == 0;
    project_restore_runtime(&pj);                 /* a project load (it stops) */
    bank_poll();
    ok &= !song.playing && t->step[0].note[0] == 40 && bank_next(0) == BK_NONE && bank_cur(0) == W_ENT;
    lv_start(LAUNCH_NOW);
    transport_req = 2;
    events_block(1);
    ok &= project_save_as(0, 0) == 0;             /* (project A: T1 on P) */
    transport_req = 1;
    events_block(1);
    t->step[1].note[0] = 77;                      /* an edit on W, playing */
    ok &= bank_switch(0, 2) == 0;
    lv_next(1);                                   /* NOW: C in, what went out (the edit with it) in launch_q, not kept */
    ok &= launch_q[0].done && sounds(81);
    project_load(0);                              /* .. a project load right then: it stops, keeps that first */
    ok &= !song.playing && !launch_q[0].done && !undo_n && bank_cur(0) == W_ENT && t->step[1].note[0] == 77;
    bad += check("  a project load: nothing staged comes in over it; right after a swap, what went out kept first", ok);

    /* staged mutes (mute_switch: the PERFORM layer's black keys) */
    lv_start(LAUNCH_LOOP);
    lv_next(5);
    mute_switch(0);
    ok = mute_arm[0] == 2u && !t->p[P_MUTE];
    lv_next(6);
    i = vage;
    ok &= sounds(51) && !t->p[P_MUTE];
    lv_next(1);                                   /* W's pass ends: muted, its step 1 starts no voice */
    ok &= t->p[P_MUTE] == 1 && !mute_arm[0] && vage == i;
    bad += check("live mute, LOOP: staged mid-pass, W plays out its pass, then silent from step 1", ok);
    lv_start(LAUNCH_BAR);
    t->p[P_MUTE] = 1;
    lv_next(5);
    mute_switch(0);
    ok = mute_arm[0] == 1u;
    lv_next(10);
    ok &= t->p[P_MUTE] == 1;
    i = vage;
    lv_next(1);                                   /* bar 2: back, its step sounds */
    ok &= !t->p[P_MUTE] && vage > i && sounds(44);
    bad += check("  unmute, BAR: on bar 2's first step, that step heard", ok);
    lv_start(LAUNCH_LOOP);
    lv_next(2);
    mute_switch(0);
    ok = bank_switch(0, 2) == 0 && mute_arm[0] == 2u;
    lv_next(9);
    i = vage;
    lv_next(1);                                   /* both on the same step: C in, muted */
    ok &= t->p[P_MUTE] == 1 && t->step[0].note[0] == 80 && vage == i;
    mute_switch(0);
    ok &= mute_arm[0] == 1u;
    transport_req = 2;
    events_block(1);
    bank_poll();                                  /* stopped before it: at once */
    ok &= !t->p[P_MUTE] && !mute_arm[0];
    bad += check("  with a pattern staged too: both on one step; stopped before its point: at once", ok);
    lv_start(LAUNCH_LOOP);
    mute_switch(0);
    mute_switch(1);
    ok = stage_cancel() && !mute_arm[0] && !mute_arm[1] && !stage_cancel();
    mute_switch(0);
    project_capture(&pj);
    project_restore_runtime(&pj);
    ok &= !mute_arm[0] && !t->p[P_MUTE];
    bad += check("  OCT- cancels them all; a song does not hold a mute back; a project load drops it", ok);
    return bad;
}

/* ---- scenes (bank.c scene_store / scene_recall; the PERFORM layer's KEYS SCENE) ---- */
static int scenes(void)
{
    int bad = 0, ok;
    uint32_t i;
    track_t *t = &trk[0];
    static project_t pj, got;
    static project_store_t w;
    fresh();
    memset(&favorites, 0, sizeof favorites);
    simple(0, 30, 12, DIR_FWD);                   /* T1: A (12 steps, notes 30..), C (8, 80..) */
    bank_put(0, 0, 0);
    simple(0, 80, 8, DIR_FWD);
    bank_put(0, 2, 0);
    simple(1, 60, 16, DIR_FWD);                   /* T2: B */
    bank_put(1, 1, 0);
    ent_go(0, 0);                                 /* T1 plays A, T2 B, T3 its own steps (no entry), T4 muted */
    simple(2, 50, 16, DIR_FWD);
    trk[3].p[P_MUTE] = 1;
    scene_store(2);
    ok = scene_used(2) && !scene_used(0) && scene_slot(2, 0) == 0u && scene_slot(2, 1) == 1u &&
         scene_slot(2, 2) == BK_NONE && scene_slot(2, 3) == BK_NONE && scene_bits(2) >> 20 == 8u && scene_now() == 2u;
    bad += check("scene 3 stored: T1 A, T2 B, T3 none (its own steps), T4 none; T4 muted", ok);
    ent_go(0, 2);                                 /* now: T1 C, T3 muted, T4 not */
    trk[2].p[P_MUTE] = 1;
    trk[3].p[P_MUTE] = 0;
    ok = scene_recall(2) == 0;
    ok &= t->step[0].note[0] == 30 && bank_cur(0) == 0u && bank_cur(1) == 1u && trk[2].step[0].note[0] == 50 &&
          !trk[2].p[P_MUTE] && trk[3].p[P_MUTE] == 1 && bank_next(0) == BK_NONE;
    bad += check("  recalled stopped: T1 A at once, T2 kept, T3's steps as they are, the MUTEs as stored", ok);
    ent_go(0, 2);
    trk[3].p[P_MUTE] = 0;
    lv_start(LAUNCH_BAR);                         /* (lv_start: W on T1, its P) */
    trk[3].p[P_MUTE] = 0;
    lv_next(5);
    ok = scene_recall(2) == 0 && bank_next(0) == 0u && bank_next(1) == BK_NONE && mute_arm[3] == 2u && stage_pending();
    lv_next(10);
    ok &= t->step[0].note[0] == 40 && !trk[3].p[P_MUTE];   /* (W still: A not yet) */
    lv_next(1);                                   /* bar 2: all of it on one step */
    bank_poll();
    ok &= t->step[0].note[0] == 30 && sounds(30) && bank_cur(0) == 0u && t->seq_idx == 0u && trk[3].p[P_MUTE] == 1 &&
          !stage_pending();
    bad += check("  recalled playing (BAR): staged, then all of it on bar 2's first step", ok);
    ok = scene_recall(5) == 1 && scene_recall(2) == 0 && bank_next(0) == BK_NONE;   /* (all as it says: nothing) */
    bad += check("  an empty scene: nothing; the scene it plays: nothing staged", ok);
    project_capture(&pj);                         /* (phase 5) the project's: in SKF2, back with a load */
    ok = proj_pack2(&w, &pj, PROJ_SKF) && proj_import(&got, &w, PROJ_SKF->size) && !memcmp(got.scene, lists.scene, PROJ_SCENES);
    memset(lists.scene, 0, sizeof lists.scene);   /* (another project's: none) */
    ok &= !scene_used(2) && !project_restore_runtime(&got) && scene_used(2) && scene_slot(2, 1) == 1u &&
          scene_bits(2) >> 20 == 8u && scene_now() == 0xFFu;
    for (i = 0; i < PROJ_SC2; i++)
        ok &= !SCENE_OLD(i);
    bad += check("  kept with the project (SKF2): a load brings them (none lit yet); the settings' old bytes free", ok);
    transport_req = 2;
    events_block(1);
    return bad;
}

/* ---- (phase 6) the last track in a scene (bank.c scene_trk: T5, T6 in 2 bytes a scene after the 50 of before); a
 * scene of four tracks (an SKF2's) leaves T5, T6 as they are; a pattern a saved SKF3 or SKF2 names is IN USE ---- */
static int scenes_six(void)
{
    int bad = 0, ok;
    uint32_t k = NTRK - 1u, id;
    static project_t pj;
    fresh();
    simple(k, 40, 8, DIR_FWD);                    /* the last track: A (40..), B (70..), on B, muted */
    bank_put(k, 0, 0);
    simple(k, 70, 8, DIR_FWD);
    bank_put(k, 1, 0);
    trk[k].p[P_MUTE] = 1;
    scene_store(6);
    ok = scene_slot(6, k) == 1u && scene_trk(6, k) == (2u | 1u << 5);
    ent_go(k, 0);
    trk[k].p[P_MUTE] = 0;
    ok &= scene_recall(6) == 0 && bank_cur(k) == 1u && trk[k].p[P_MUTE] && trk[k].step[0].note[0] == 70u;
    bad += check("(phase 6) the last track in a scene: its entry and MUTE stored and recalled", ok);
    lists.scene[50u + 2u * 6u] = lists.scene[51u + 2u * 6u] = 0;   /* (scene 7 as an SKF2 brings it) */
    ent_go(k, 0);
    trk[k].p[P_MUTE] = 0;
    ok = scene_trk(6, k) == SCENE_NOT && scene_recall(6) == 0 && bank_cur(k) == 0u && !trk[k].p[P_MUTE];
    bad += check("  a scene of four tracks (an SKF2's): T5, T6 left as they are", ok);
    id = ent_pat(k, 1);
    ok = project_save_as(0, 0) == 0;              /* (SKF3 with six tracks: the last track's list in it) */
    lists_clear();
    detach(k);
    ok &= pool_refs(id) == 1u;
    pool_clear(id);
    ok &= str_eq(ui.msg, "IN USE") && pool_used(id);
    memset(proj_slot, 0, sizeof proj_slot);
    project_capture(&pj);
    pj.ent[0][0][0] = (uint8_t)(id + 1u);         /* (T1's entry A naming it, in an SKF2) */
    ok &= proj_pack2(&proj_slot[2], &pj, &PROJ_SKF2) && pool_refs(id) == 1u;
    pool_clear(id);
    ok &= str_eq(ui.msg, "IN USE") && pool_used(id);
    bad += check("  a pattern a saved project names (the last track's list in its format; T1's in an SKF2): IN USE", ok);
    return bad;
}

/* ---- (phase 5) the song (bank.c song_poll, song_top; launch.c launch_due's repeats, launch_ahead): every track
 * stepped one step a block, the main loop's frame before each ---- */
static void sg_frame(void) { bank_poll(); song_poll(); }
static void sg_steps(uint32_t n)
{
    while (n--) {
        sg_frame();
        lv_next(1);
    }
}
static int heard(uint32_t k, uint32_t note) { return trk[k].seq_n && trk[k].seq_notes[0] == note; }
static void sg_play(void) { transport_req = 1; events_block(1); }
static void sg_stop(void) { transport_req = 2; events_block(1); sg_frame(); }
static int song_test(void)
{
    int bad = 0, ok;
    uint32_t i;
    track_t *t = &trk[0];
    static uint8_t r[BK_REC];
    fresh();
    ui_launch = LAUNCH_LOOP;                      /* (a launch's: at the pass end) */
    simple(0, 30, 4, DIR_FWD);                    /* T1: A 4 steps (30..) x2, B 2 (50..), C empty, D 3 (70..) x3 */
    bank_put(0, 0, 0);
    simple(0, 50, 2, DIR_FWD);
    bank_put(0, 1, 0);
    simple(0, 70, 3, DIR_FWD);
    bank_put(0, 3, 0);
    lists.ent[0][0][1] = 1;
    lists.ent[0][3][1] = 2;
    simple(1, 90, 3, DIR_FWD);                    /* T2: X 3 steps (90..), Y 5 (100..) */
    bank_put(1, 0, 0);
    simple(1, 100, 5, DIR_FWD);
    bank_put(1, 1, 0);
    simple(2, 60, 4, DIR_FWD);                    /* T3: one entry */
    bank_put(2, 0, 0);
    ent_go(0, 0);
    ent_go(1, 0);
    lists.song = 1;
    sg_play();
    ok = heard(0, 30) && heard(1, 90);
    sg_steps(7);                                  /* step 7: A's last, its second pass; T2: Y's last */
    ok &= heard(0, 33) && bank_cur(0) == 0u && bank_next(0) == 1u && heard(1, 104) && !stage_pending();
    sg_steps(1);
    ok &= heard(0, 50) && heard(1, 90);
    bad += check("song: A x2, then B; each track at its own pass end (T2: X Y X ..); the next staged ahead", ok);
    t->step[1].note[0] = 55;                      /* (an edit on B as it plays) */
    sg_steps(1);
    ok = heard(0, 55);
    sg_steps(1);                                  /* ((1.2.1, #12) C, empty between B and D: a rest, B's LEN 2 x1) */
    ok &= !trk[0].seq_n;
    sg_steps(1);                                  /* (its frame took the swap in: on C, on no pattern, D staged) */
    ok &= !trk[0].seq_n && lists.cur[0] == 2u && !trk_pid[0] && bank_next(0) == 3u;
    sg_steps(1);
    ok &= heard(0, 70);
    sg_steps(8);                                  /* D x3 */
    ok &= heard(0, 72);
    sg_steps(1);
    ok &= heard(0, 30) && !launch_q[2].armed && heard(2, 60 + 21u % 4u);
    bad += check("  .. C (empty, between B and D) rests B's length, D x3, then A (the list loops); T3's one entry: never a swap", ok);
    sg_steps(8);                                  /* A x2 */
    ok = heard(0, 50);
    sg_steps(1);
    ok &= heard(0, 55);
    bad += check("  B again: its edit there (kept when it went out)", ok);
    ok = !stage_cancel() && launch_q[0].armed;    /* (C staged: the song's rest) */
    lists.song = 0;
    sg_frame();
    ok &= !launch_q[0].armed;
    lists.song = 1;
    sg_frame();
    ok &= launch_q[0].armed && bank_next(0) == 2u;
    bad += check("  OCT- leaves the song's next (none of the user's); SONG off drops it, on stages it again", ok);
    sg_stop();
    ok = !launch_q[0].armed && bank_cur(0) == 1u && heard(0, 55) == 0;
    sg_play();                                    /* PLAY: on B, the entry it is on, from its start */
    sg_steps(1);
    ok &= heard(0, 55);
    sg_steps(2);                                  /* (C's rest) */
    ok &= !trk[0].seq_n;
    sg_steps(1);
    ok &= heard(0, 70);
    bad += check("  STOP: the song's next dropped; PLAY: on from the entry each track is on", ok);
    ok = bank_switch(0, 1) == 0 && !launch_q[0].adv;   /* (a launch, LOOP: B at D's first pass end, not its third) */
    sg_steps(3);
    ok &= heard(0, 50);
    sg_steps(2);
    ok &= !trk[0].seq_n;                          /* (C's rest) */
    sg_steps(2);
    ok &= heard(0, 70);
    bad += check("  a launch moves the song: B at D's pass end, then on from B (C's rest, D)", ok);
    sg_steps(1);
    song_top();                                   /* GLO + PLAY: each track's first entry, from the top */
    events_block(1);
    ok = song.playing && heard(0, 30) && heard(1, 90) && heard(2, 60) && bank_cur(0) == 0u && bank_cur(1) == 0u;
    bad += check("  GLO + PLAY: from the top, every track on its first entry", ok);

    sg_stop();                                    /* no room to keep what would go out: the song waits */
    simple(0, 30, 4, DIR_FWD);
    bank_put(0, 0, 0);
    lists.ent[0][0][1] = 0;
    ok = bk_pack(r, &trk[3]);
    for (i = 0; i < PEND_N; i++)
        ok &= pat_keep_rec(40u + i, r) == 0;
    sg_play();
    sg_steps(8);
    ok &= heard(0, 30) && !launch_q[0].armed;    /* (A again: B not staged) */
    sg_stop();
    ok &= keep_all() == 0;                        /* (the autosave's round: written) */
    sg_play();
    sg_steps(4);
    ok &= heard(0, 50);
    bad += check("  no waiting slot to keep what goes out: the entry plays on; written (stopped), on again", ok);

    sg_stop();                                    /* a LEN 1 pattern at 1/32, x3: exactly three steps */
    simple(0, 40, 1, DIR_FWD);
    t->p[P_SDIV] = 3;
    bank_put(0, 0, 0);
    lists.ent[0][0][1] = 2;
    simple(0, 45, 1, DIR_FWD);
    t->p[P_SDIV] = 3;
    bank_put(0, 1, 0);
    lists.ent[0][1][1] = 0;
    lists.ent[0][3][0] = 0;                       /* (D out of the list) */
    ent_go(0, 0);
    sg_play();
    ok = heard(0, 40);
    for (i = 1; i < 12u; i++) {
        sg_steps(1);
        ok &= heard(0, i % 4u == 3u ? 45u : 40u);
    }
    bad += check("  LEN 1 at 1/32, x3: P P P Q P P P Q, exact", ok);

    sg_stop();                                    /* NUDGE: the next step early in this one, but not across a swap */
    simple(0, 30, 4, DIR_FWD);
    t->step[0].nudge = -16;
    bank_put(0, 0, 0);
    lists.ent[0][0][1] = 1;
    simple(0, 50, 2, DIR_FWD);
    bank_put(0, 1, 0);
    ent_go(0, 0);
    sg_play();
    sg_steps(3);                                  /* A's last step, pass 1: its step 1 set to play early in it */
    ok = (t->nud & NUD_EARLY) && t->early_idx == 0u;
    sg_steps(4);                                  /* pass 2's last (B comes in at its end): not */
    ok &= !(t->nud & NUD_EARLY);
    sg_steps(1);
    ok &= heard(0, 50);
    bad += check("  NUDGE: a step early in the one before, not where the next pattern comes in", ok);
    sg_stop();
    lists.song = 0;
    return bad;
}

/* ---- (phase 5) projects of before SKF2 (bank.c proj_migrate): their tracks pool patterns, SKF2 ---- */
static uint32_t pool_count(void)
{
    uint32_t id, n = 0;
    for (id = 0; id < BK_POOL; id++)
        n += (uint32_t)pool_used(id);
    return n;
}
static int migrate(void)
{
    int bad = 0, ok, n;
    uint32_t k, cuts = 0;
    static uint8_t before[sizeof nor];
    static project_t pj, got;
    static project_store_t w, ref;
    static uint8_t r[BK_REC];
    snap_t a[NTRK], b;
    fresh();
    rich(0, 5);                                   /* T1 every field; T2 the same pattern (on its own track); T3 empty; */
    rich(1, 5);                                   /* T4 8 steps */
    simple(3, 50, 8, DIR_FWD);
    for (k = 0; k < NTRK; k++)
        snap(&a[k], k);
    project_capture(&pj);
    ok = proj_pack(&w, &pj) && !st_save(OBJ_PROJECT0, &w, sizeof w) && !st_save(OBJ_PROJECT0 + 2u, &w, sizeof w) &&
         !st_save(OBJ_AUTOSAVE, &w, sizeof w);    /* (SKF1: slots A and C, the autosave) */
    SCENE_OLD(0) = 7;                             /* (an old scene in the settings; (phase 6) the favorites after) */
    SCENE_OLD(PROJ_SC2) = 9;
    memcpy(before, nor, sizeof nor);
    ok &= proj_migrate(0) == 0 && !pool_count() && SCENE_OLD(0) == 7;
    bad += check("migrate: not after a crash, a hang or a failed boot (nothing written)", ok);
    ok = proj_migrate(1) == 1 && pool_count() == 2u && !SCENE_OLD(0) && SCENE_OLD(PROJ_SC2) == 9;
    for (k = 0; k < 5u; k++) {
        uint32_t obj = k < 4u ? OBJ_PROJECT0 + k : OBJ_AUTOSAVE;
        n = st_load(obj, &w, sizeof w);
        if (k == 1u || k == 3u) {
            ok &= n <= 0;                         /* (empty slots: none) */
            continue;
        }
        ok &= n == (int)PROJ_SKF->size && proj_import(&got, &w, n) && got.ent[0][0][0] && got.ent[1][0][0] == got.ent[0][0][0] &&
              !got.ent[2][0][0] && got.ent[3][0][0] && got.ent[3][0][0] != got.ent[0][0][0] && !got.cur[0] && !got.ent[0][1][0];
    }
    ok &= proj_migrate(1) == 0;                   /* (again: all SKF2 (SKF3 with six tracks), nothing to do) */
    bad += check("  slots A C and the autosave: SKF2 (SKF3), each track's entry A its pattern (T1 T2 one, shared), T3 none; "
                 "the settings' old scenes freed (their 50 bytes: the favorites after stay)", ok);
    favorite_set(ENGI_DRUMX, 0, 1);               /* ((Skiff phase 7) row 14 is DRUM-X's favourites: freed once only) */
    ok = proj_migrate(1) == 0 && favorite_has(ENGI_DRUMX, 0) && SCENE_FREED == SCENE_FREED_V;
    favorite_set(ENGI_DRUMX, 0, 0);
    bad += check("  the old scenes freed once: a DRUM-X favourite (row 14) stays", ok);
    for (k = 0; k < NTRK; k++)
        detach(k);
    project_load(0);
    ok = 1;
    for (k = 0; k < NTRK; k++) {
        snap(&b, k);
        ok &= same(&a[k], &b);
    }
    bad += check("  the same music: every track's steps, timing, automation and locks as before", ok);
    st_load(OBJ_PROJECT0, &ref, sizeof ref);
    ok = 1;
    for (n = 0; n < 400; n++) {                   /* the power goes after n writes (each page, each commit) */
        memcpy(nor, before, sizeof nor);
        power_cycle();
        fail_after = n;
        proj_migrate(1);
        k = fail_after > 0;                       /* (it ended before the cut: done) */
        cuts += !k;
        fail_after = -1;
        power_cycle();
        proj_migrate(1);                          /* (the next boot) */
        ok &= st_load(OBJ_PROJECT0, &w, sizeof w) == (int)PROJ_SKF->size && !memcmp(&w, &ref, PROJ_SKF->size) &&
              pool_count() == 2u && st_load(OBJ_AUTOSAVE, &w, sizeof w) == (int)PROJ_SKF->size;
        if (k)
            break;
    }
    bad += check("  a power cut after any write: the next boot ends it the same (the same patterns, none twice)",
                 ok && cuts > 10u && n < 400);
    memcpy(nor, before, sizeof nor);
    power_cycle();
    for (k = 0; k < NTRK; k++)
        detach(k);
    ok = bk_pack(r, &trk[2]);                     /* (an empty track's pattern: none of the project's) */
    for (k = 0; k < BK_POOL; k++)
        ok &= pool_put(k, r) == 0;
    ok &= proj_migrate(1) == 0 && st_load(OBJ_PROJECT0, &w, sizeof w) == (int)sizeof w;
    memset(proj_slot, 0, sizeof proj_slot);       /* (the RAM copies lost: from flash) */
    proj_fetch(0);
    project_load(0);
    for (k = 0; k < NTRK; k++) {
        snap(&b, k);
        ok &= same(&a[k], &b);
    }
    bad += check("  the pool full: the projects as they were (SKF1), loading with their own steps", ok);
    return bad;
}

/* ---- (Skiff phase 7) the DRUM-X kit is the project's: saved with it (SKF3), back when loaded, the factory's for a project
 * without one; an edit of it is something an autosave keeps ---- */
static int dx_kit_project(void)
{
    int bad = 0, ok;
    uint32_t sig;
    fresh();
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    sig = autosave_sig();
    dx_kit[DV_KICK].a[DXP_DECAY] = 5;
    ok = autosave_sig() != sig && project_save_as(1, "KIT") == 0;
    dx_kit[DV_KICK].a[DXP_DECAY] = 99;
    project_load(1);
    if (NTRK > PROJ_NT) {
        ok &= dx_kit[DV_KICK].a[DXP_DECAY] == 5;
        bad += check("(phase 7) the DRUM-X kit: saved with the project (SKF3), back when it loads; an edit an autosave keeps", ok);
    } else {
        ok &= !memcmp(dx_kit, DX_KIT_DEF, sizeof dx_kit);
        bad += check("(phase 7) the DRUM-X kit with four tracks (SKF2 holds none): the factory's when a project loads", ok);
    }
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    return bad;
}


/* (PERNOTE, fork 1.3.3-notes) each note's own mode (core.h step_t.nmode) in the pool: a pattern with modes is a version 3
 * record and comes back whole over a power cycle; one without is version 1 / 2 with its plane 0 (as before); every one of
 * the 625 mode numbers goes through bk_put10 / bk_get10; a chunk an earlier build wrote (3552 bytes, 888-byte records)
 * still loads and is written in today's size when next saved; backups of both sizes restore; an earlier build's chunk
 * never holds a version 3 record */
static void modes_on(uint32_t k, uint32_t seed)   /* track k's notes get modes (some steps none) */
{
    uint32_t i, j;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &trk[k].step[i];
        nm_clear(s);
        for (j = 0; j < s->n && (i + seed) % 3u; j++)
            nm_set(s, j, (i * 3u + j * 7u + seed) % NM_COUNT);
    }
}
static void chunk_old(uint32_t c, const uint8_t *buf1)   /* chunk c's next copy as an earlier build writes it */
{
    uint32_t to = bk_free(), base = bk_sector(to), off;
    bk_hdr_t h;
    bk_erase(base);
    for (off = 0; off < BK_LEN1; off += 256u)
        bk_prog(base + 256u + off, buf1 + off, BK_LEN1 - off > 256u ? 256u : BK_LEN1 - off);
    h.magic = BK_MAGIC; h.chunk = c; h.seq = bk.top + 1u; h.len = BK_LEN1; h.sum = proj_hash(buf1, BK_LEN1);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu; h.hsum = proj_hash(&h, sizeof h - 4u);
    bk_prog(base, &h, sizeof h);
}
static int pernote_bank(void)
{
    int bad = 0, ok = 1;
    snap_t a, b, c;
    static uint8_t r[BK_REC], old[BK_LEN1], back[BK_LEN];
    uint32_t i, id, ch, len;
    fresh();
    rich(1, 3);
    modes_on(1, 3);
    snap(&a, 1);
    ok = bank_put(1, 5, 0) == 0;
    id = ent_pat(1, 5);
    ok &= !pat_read(id, r) && r[0] == BK_VER3;
    power_cycle();
    detach(1);
    ok &= ent_go(1, 5) == 0;
    snap(&b, 1);
    bad += check("(PERNOTE) a pattern with note modes: version 3, every mode back over a power cycle", ok && same(&a, &b));
    rich(2, 7);
    snap(&c, 2);
    ok = bank_put(2, 3, 0) == 0 && !pat_read(ent_pat(2, 3), r) && (r[0] == BK_VER || r[0] == BK_VER2);
    for (i = BK_PLANE; i < BK_REC; i++)
        ok &= !r[i];
    detach(2);
    ok &= ent_go(2, 3) == 0;
    snap(&b, 2);
    bad += check("  one without: version 1 or 2 as before, its plane 0, back whole", ok && same(&c, &b));
    {   /* every mode number, on a step of each n, through the version 3 step coding */
        static uint8_t q[BK_REC];
        step_t s, t;
        uint32_t code, n;
        ok = 1;
        for (n = 1; n <= 4u; n++)
            for (code = 0; code < 625u; code++) {
                memset(&s, 0, sizeof s);
                s.n = (uint8_t)n; s.time = (uint8_t)(code % 3u); s.vel = 90; s.note[0] = 60; s.note[1] = 62;
                s.note[2] = 65; s.note[3] = 69; s.flags = (uint8_t)(code % 4u);
                nm_uncode(&s, code);
                nm_fit(&s);
                memset(q, 0, sizeof q);
                q[0] = BK_VER3;
                ok &= bk_put10(q, code % NSTEP, &s);
                memset(&t, 0, sizeof t);
                ok &= bk_get10(&t, q, code % NSTEP) && !memcmp(&s, &t, sizeof s);
            }
        bad += check("  all 625 mode numbers, n 1..4, every TIME and flag: through bk_put10 / bk_get10 unchanged", ok);
    }
    /* an earlier build's chunk: the record of T3's D (version 1 / 2, 888 bytes) at its place, 888 apart */
    id = ent_pat(2, 3);
    ch = bk_chunk(id);
    ok = !pat_read(id, r);
    memset(old, 0, sizeof old);
    memcpy(old + id % BK_PER * BK_REC1, r, BK_REC1);
    chunk_old(ch, old);
    power_cycle();
    ok &= bk.leg[ch] == 1u && pool_used(id);
    detach(2);
    ok &= ent_go(2, 3) == 0;
    snap(&b, 2);
    bad += check("  a chunk an earlier build wrote (3552 bytes): it loads, its pattern whole", ok && same(&c, &b));
    trk_pid[2] = 0;                                /* (the track a new pattern: its next keep takes a new id) */
    modes_on(2, 1);
    snap(&a, 2);
    ok = bank_put(2, 4, 0) == 0 && ent_pat(2, 4) != id;
    ok &= bk.leg[bk_chunk(ent_pat(2, 4))] == 0u;
    power_cycle();
    detach(2);
    ok &= ent_go(2, 4) == 0;
    snap(&b, 2);
    ok &= same(&a, &b);
    detach(2);
    ok &= ent_go(2, 3) == 0;
    snap(&b, 2);
    bad += check("  saved again: today's size, the old pattern and the new one with modes both there", ok && same(&c, &b));
    /* backups: a chunk read out is today's size; an earlier build's 3552-byte chunk restores */
    {
        const uint8_t *p = bank_chunk(ch, &len);
        ok = len == BK_LEN;
        memcpy(back, p, BK_LEN);
        ok &= bank_chunk_ok(back, BK_LEN) && bank_chunk_ok(old, BK_LEN1) && !bank_chunk_ok(old, 1000u);
        ok &= bank_restore(ch, old, BK_LEN1) == 0 && bk.leg[ch] == 0u;
        detach(2);
        ok &= ent_go(2, 3) == 0;
        snap(&b, 2);
        ok &= same(&c, &b);
        old[id % BK_PER * BK_REC1] = BK_VER3;
        ok &= !bank_chunk_ok(old, BK_LEN1);
        bad += check("  backups: read out at 3840; an earlier build's 3552 restores; none holds version 3 at 888", ok);
    }
    return bad;
}

int main(void)
{
    int bad = layout() + fidelity() + keep() + entries() + staged_all() + last_track() + torn() + wear_and_wrap() +
              playing() + live() + scenes() + scenes_six() + song_test() + migrate() + dx_kit_project() + pernote_bank();
    printf("%s\n", bad ? "BANK TEST FAILED" : "pattern bank tests passed");
    return bad != 0;
}
