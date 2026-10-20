/* SPDX-License-Identifier: GPL-3.0-only */
/* Skiff: the pattern pool (DESIGN.md SAVE > PATTERNS). Sixty-four patterns, kept in flash exactly as they were played:
 * every step whole (its notes and chord, velocity, accent, slide, chance, RATCH or FLAM, trig condition, NUDGE, lane
 * hits and their accents), LEN DIV SWING GATE and DIR, and the track's automation and locks. (Phase 5) Each track's
 * list (A..P, ui.c lists) names pool patterns, a project its lists (SKF2): the patterns are every project's. What a
 * track has is kept as the pattern it is on, by itself: waiting in RAM (PEND_N), written at the autosave's quiet stop.
 *
 * A pattern is BK_REC bytes:
 *   0          the version (0: empty)
 *   1          the parameter count its motion's ids are of (project.c proj_motion_ids): (Skiff phase 9) PROJ_NP,
 *              which firmware before reads, unless the motion names a parameter added since: then P_COUNT
 *   2..6       LEN DIV SWING GATE DIR, each + 64 (the sound's own values, not an automated one)
 *   7          its motion records (0..64), bit 7: the automation plays (motion.on)
 *   8..583     the 64 steps, 9 bytes each (project.c step_put9)
 *   584..631   their NUDGE (core.h nudge_pack)
 *   632..887   the motion, 64 x (step (bit 6: the id's bit 7), id (bit 7: a lock), value low, value high)
 * (Skiff 1.3) A record whose motion names an id from 128 on (P_COUNT passed 127: only one written with r[1] = P_COUNT,
 * a parameter added since phase 9 named) is version 2 (BK_VER2), its ids' bit 7 in bit 6 of their step byte; every
 * other one version 1 as before, which firmware before reads (it shows a version 2 as empty)
 * Phase 5: the records are one pool, ids 0..BK_POOL-1 (pool_put, pat_decode); id = track x 16 + slot, so a chunk is
 * still one track's A..D, E..H, I..L or M..P and every bank saved before reads the same by id.
 * Flash: BK_CHUNKS chunks of BK_PER patterns (ids 4c .. 4c+3), each written whole to one
 * of BK_NSECT sectors (fm1_flash.h FL_BANK, FL_USR: 0xE7000..0xE8FFF, 0xEA000..0xFBFFF) that holds no chunk's current
 * copy: the payload from offset 256, then the commit record at 0 (its own magic, the chunk, a sequence, the payload's
 * length and hash, the record's hash) last, so a write torn anywhere leaves the chunk's copy before in charge. At boot
 * each chunk's newest copy whose payload checks wins. Four sectors spare: saves go round the free ones (wear). No RAM
 * image: a pattern is read when it is loaded (SPI, short IRQ-off windows: playing goes on); a write reads its chunk,
 * changes its patterns and writes it, only while stopped (an erase mutes the audio).
 * Without the flash driver (FELUCCA_FLASH 0) BK_RAM keeps the sectors in RAM (the host's UI tests); else no bank.
 *
 * (PERNOTE, fork 1.3.3-notes, rev 1; SPEC-PERNOTE.md "Storage") A record is 960 bytes (BK_REC), a chunk 3840 (BK_LEN, the
 * most a sector holds after its 256-byte commit page). Bytes 0..887 are the record as above; 888..959 the note-mode
 * plane. A record whose steps hold no note mode (core.h step_t.nmode) is written as version 1 or 2, byte for byte as
 * before, its plane 0. One that holds some is version 3 (BK_VER3): its motion as version 2's, each step's 4 modes one
 * number 0..624 (core.h nm_code), its low 9 bits in the plane (64 x 9 bits, low bit first) and its bit 9 in bit 6 of the
 * step's meta byte, which in version 3 holds n x 3 + TIME in bits 0..3 and the step flags in 4..5 (bit 7 FLAM as
 * before: bk_put10 / bk_get10). A chunk an earlier build wrote (3552 bytes: 4 x 888, BK_LEN1) still reads: bk_leg says
 * which chunks are so, and every read spreads its records out to 960 (their planes 0). The next save of the chunk writes
 * it in the new size. Stock Skiff reads a version 3 record as empty and refuses a 3840-byte chunk (its copy before
 * stays in charge there). */
#define BK_SLOTS 16u
#define BK_VER 1u
#define BK_VER2 2u                                 /* (Skiff 1.3) an id from 128 on (see the top) */
#define BK_VER3 3u                                 /* (PERNOTE) note modes (see the top) */
static int bk_ver(uint32_t v) { return v == BK_VER || v == BK_VER2 || v == BK_VER3; }
#define BK_PER 4u
#define BK_POOL NPAT                               /* the patterns, ids 0 .. 63 (every track's: phase 5) */
#define BK_CHUNKS (BK_POOL / BK_PER)
#define BK_STEPS 8u
#define BK_NUDGE (BK_STEPS + NSTEP * 9u)
#define BK_MOT (BK_NUDGE + NSTEP / 4u * 3u)
#define BK_REC1 (BK_MOT + MOTION_MAX * 4u)        /* (PERNOTE) a record as every build before wrote it: 888 */
#define BK_PLANE BK_REC1                           /* (PERNOTE) the note-mode plane: 9 bits a step */
#define BK_REC (BK_PLANE + NSTEP * 9u / 8u)        /* (PERNOTE) a record: 960 */
#define BK_LEN (BK_PER * BK_REC)                   /* a chunk's payload: 3840 */
#define BK_LEN1 (BK_PER * BK_REC1)                 /* (PERNOTE) a chunk an earlier build wrote: 3552 */
#define BK_NSECT 20u
#define BK_NONE 0xFFu
#define BK_MAGIC 0x31424B53u                       /* "SKB1" */
_Static_assert(BK_REC1 == 888u && BK_REC == 960u && BK_LEN <= 4096u - 256u && BK_CHUNKS + 4u == BK_NSECT,
               "pattern bank layout");
typedef struct { uint32_t magic, chunk, seq, len, sum, rsv[2], hsum; } bk_hdr_t;   /* at a sector's 0 */
static const uint8_t BK_TIME[5] = {P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_DIR};

static struct {
    uint8_t sect[BK_CHUNKS];                       /* each chunk's current sector + 1 (0: never saved, all empty) */
    uint32_t top;                                  /* the newest sequence (a save: + 1) */
    uint8_t next;                                  /* the sector a save tries first (round: wear) */
    uint8_t len[BK_POOL];                          /* each pattern's LEN, 0 = empty (the page) */
    uint32_t gen;                                  /* any change (the page redraws) */
    uint8_t leg[BK_CHUNKS];                        /* (PERNOTE) 1: the chunk's current copy is an earlier build's, 3552
                                                    * bytes (BK_LEN1): its records 888 bytes apart */
} bk;
static uint8_t bk_buf[BK_LEN] __attribute__((section(".pool")));   /* a chunk read, changed and written */
static uint8_t bk_rec[BK_REC] __attribute__((section(".pool")));   /* a track's pattern packed (pool_put) */
/* (phase 5) keeping edits: a pattern changed while playing waits here for the next quiet stop (pat_flush_one), and every
 * read of it sees this copy first. PEND_N records, each its pool id (PAT_NONE: the slot free); a record all 0: the
 * pattern is to be emptied */
#define PEND_N NTRK                                /* (one a track: every track can have one staged, stage_entry) */
static uint8_t pend_id[PEND_N] __attribute__((section(".pool")));   /* (.pool: not among the globals the audio ISR reaches) */
static uint8_t pend_rec[PEND_N][BK_REC] __attribute__((section(".pool")));
/* each track's pattern as it came in or was last kept (a hash of it packed): a copy unchanged since is not kept, so
 * another track on the same pattern that changed it is not undone; the staged one's, the track's when it comes in */
static uint32_t trk_sig[NTRK] __attribute__((section(".pool")));
static uint32_t q_sig[NTRK] __attribute__((section(".pool")));
_Static_assert(BK_POOL == NPAT, "the pool is the bank's records");
static uint8_t bk_bufc;                            /* the chunk + 1 bk_buf holds whole (0: none, or changed; a backup's
                                                    * LIST reads a chunk in, its GETs read on: bank_chunk) */

static uint32_t bk_sector(uint32_t i) { return i < 2u ? 0xE7000u + i * 0x1000u : 0xEA000u + (i - 2u) * 0x1000u; }
static uint32_t bk_chunk(uint32_t id) { return id % BK_POOL / BK_PER; }

#if FELUCCA_FLASH
#define BK_ON flash_ok
static int bk_read(uint32_t off, void *d, uint32_t n) { return st_read(off, d, n); }
static int bk_erase(uint32_t off) { return st_erase(off); }
static int bk_prog(uint32_t off, const void *s, uint32_t n) { return st_prog(off, s, n); }
#elif defined(BK_RAM)
#define BK_ON 1
static uint8_t bk_ram[BK_NSECT][0x1000];
static uint8_t *bk_at(uint32_t off)
{
    uint32_t i = off < 0xEA000u ? (off - 0xE7000u) >> 12 : ((off - 0xEA000u) >> 12) + 2u;
    return &bk_ram[i % BK_NSECT][off & 0xFFFu];
}
static int bk_read(uint32_t off, void *d, uint32_t n) { memcpy(d, bk_at(off), n); return 0; }
static int bk_erase(uint32_t off) { memset(bk_at(off), 0xFF, 0x1000u); return 0; }
static int bk_prog(uint32_t off, const void *s, uint32_t n)
{
    uint8_t *p = bk_at(off);
    for (uint32_t i = 0; i < n; i++) p[i] &= ((const uint8_t *)s)[i];
    return 0;
}
#else
#define BK_ON 0
static int bk_read(uint32_t off, void *d, uint32_t n) { (void)off; (void)d; (void)n; return -1; }
static int bk_erase(uint32_t off) { (void)off; return -1; }
static int bk_prog(uint32_t off, const void *s, uint32_t n) { (void)off; (void)s; (void)n; return -1; }
#endif

static int bk_head(uint32_t i, bk_hdr_t *h)        /* sector i's commit record valid: 0 */
{
    if (bk_read(bk_sector(i), h, sizeof *h))
        return -1;
    return h->magic == BK_MAGIC && h->chunk < BK_CHUNKS && (h->len == BK_LEN || h->len == BK_LEN1) &&
           h->hsum == proj_hash(h, sizeof *h - 4u) ? 0 : -1;
}
/* (PERNOTE) the 4 records of an earlier build's chunk (in b: 888 bytes apart) moved 960 apart, their planes 0 */
static void bk_spread(uint8_t *b)
{
    uint32_t r;
    for (r = BK_PER; r-- > 0;) {
        memmove(b + r * BK_REC, b + r * BK_REC1, BK_REC1);
        memset(b + r * BK_REC + BK_REC1, 0, BK_REC - BK_REC1);
    }
}
static int bk_body(uint32_t i, const bk_hdr_t *h)  /* its payload -> bk_buf, the hash right: 0 */
{
    if (bk_read(bk_sector(i) + 256u, bk_buf, h->len) || proj_hash(bk_buf, h->len) != h->sum)
        return -1;
    if (h->len == BK_LEN1)                         /* ((PERNOTE) an earlier build's: spread out) */
        bk_spread(bk_buf);
    return 0;
}
/* (PERNOTE) chunk c's current copy -> bk_buf, as 960-byte records (an earlier build's spread out): 0 read */
static int bk_read_chunk(uint32_t c)
{
    if (bk_read(bk_sector(bk.sect[c] - 1u) + 256u, bk_buf, bk.leg[c] ? BK_LEN1 : BK_LEN))
        return -1;
    if (bk.leg[c])
        bk_spread(bk_buf);
    return 0;
}
static int bk_newer(uint32_t a, uint32_t b) { return a != b && a - b < 0x80000000u; }   /* (wraps) */
static void bk_lens(uint32_t c)                    /* chunk c's patterns (in bk_buf): their LENs for the page */
{
    uint32_t r;
    for (r = 0; r < BK_PER; r++) {
        const uint8_t *p = bk_buf + r * BK_REC;
        int32_t len = (int32_t)p[2] - 64;
        bk.len[c % BK_CHUNKS * BK_PER + r] = (uint8_t)(!bk_ver(p[0]) ? 0 : len < 1 ? 1 : len > (int32_t)NSTEP ? NSTEP : len);
    }
}

/* persist_boot (main.c): each chunk's newest copy whose payload checks (a torn or damaged one: the copy before) */
static void bank_boot(void)
{
    bk_hdr_t h[BK_NSECT];
    uint8_t ok[BK_NSECT];
    uint32_t i, c;
    memset(&bk, 0, sizeof bk);
    memset(pend_id, PAT_NONE, sizeof pend_id);     /* (phase 5: nothing waiting to be kept) */
    bk_bufc = 0;
    if (!BK_ON)
        return;
    for (i = 0; i < BK_NSECT; i++) {
        ok[i] = bk_head(i, &h[i]) == 0;
        if (ok[i] && (bk.top == 0u || bk_newer(h[i].seq, bk.top)))
            bk.top = h[i].seq;
    }
    for (c = 0; c < BK_CHUNKS; c++)
        for (;;) {
            uint32_t best = BK_NONE;
            for (i = 0; i < BK_NSECT; i++)
                if (ok[i] && h[i].chunk == c && (best == BK_NONE || bk_newer(h[i].seq, h[best].seq)))
                    best = i;
            if (best == BK_NONE)
                break;
            if (bk_body(best, &h[best]) == 0) {
                bk.sect[c] = (uint8_t)(best + 1u);
                bk.leg[c] = h[best].len == BK_LEN1;   /* ((PERNOTE) an earlier build's size) */
                bk_lens(c);
                break;
            }
            ok[best] = 0;
        }
}

static uint32_t bk_free(void)                      /* a sector holding no chunk's current copy, from bk.next on */
{
    uint32_t i, j, used;
    for (i = 0; i < BK_NSECT; i++) {
        uint32_t s = (bk.next + i) % BK_NSECT;
        for (used = 0, j = 0; j < BK_CHUNKS; j++)
            used |= bk.sect[j] == s + 1u;
        if (!used)
            return s;
    }
    return BK_NONE;
}
/* bk_buf -> chunk c's next copy, the commit record last, then read back: 0 written */
static int bk_write(uint32_t c)
{
    uint32_t to = bk_free(), base, off;
    bk_hdr_t h, chk;
    int rc;
    if (to == BK_NONE)
        return -1;
    base = bk_sector(to);
    if ((rc = bk_erase(base)) != 0)
        return rc;
    for (off = 0; off < BK_LEN; off += 256u)
        if ((rc = bk_prog(base + 256u + off, bk_buf + off, BK_LEN - off > 256u ? 256u : BK_LEN - off)) != 0)
            return rc;
    h.magic = BK_MAGIC;
    h.chunk = c;
    h.seq = bk.top + 1u;
    h.len = BK_LEN;
    h.sum = proj_hash(bk_buf, BK_LEN);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hsum = proj_hash(&h, sizeof h - 4u);
    if ((rc = bk_prog(base, &h, sizeof h)) != 0)
        return rc;
    if (bk_head(to, &chk) || memcmp(&chk, &h, sizeof h) || bk_body(to, &chk))
        return -7;                                 /* (a part that does not keep what it is given: not SAVED) */
    bk.sect[c] = (uint8_t)(to + 1u);
    bk.leg[c] = 0;                                 /* ((PERNOTE) written in the new size) */
    bk.top = h.seq;
    bk.next = (uint8_t)((to + 1u) % BK_NSECT);
    bk.gen++;
    return 0;
}

/* (PERNOTE) a version 3 record's step i (see the top): step_put9's 9 bytes, the meta byte n x 3 + TIME | flags << 4 |
 * mode bit 9 << 6 | FLAM << 7, the mode number's low 9 bits into the plane. 0: a step no format holds */
static int bk_put10(uint8_t *r, uint32_t i, const step_t *st)
{
    uint8_t *b = r + BK_STEPS + i * 9u;
    uint32_t c = nm_code(st), bit = i * 9u, k;
    if (!step_put9(b, st))
        return 0;
    b[4] = (uint8_t)((st->n * 3u + st->time) | (st->flags & 3u) << 4 | ((c >> 9) & 1u) << 6 | (b[4] & 128u));
    for (k = 0; k < 9u; k++, bit++)
        if ((c >> k) & 1u)
            r[BK_PLANE + bit / 8u] |= (uint8_t)(1u << (bit % 8u));
    return 1;
}
/* (PERNOTE) record r's step i -> *s (its nudge untouched), as its version stores it: 0 for bytes no build wrote */
static int bk_get10(step_t *s, const uint8_t *r, uint32_t i)
{
    const uint8_t *b = r + BK_STEPS + i * 9u;
    uint8_t v1[9];
    uint32_t meta, nt, c = 0, bit = i * 9u, k;
    if (r[0] != BK_VER3) {
        if (!step_get9(s, b))
            return 0;
        nm_clear(s);
        return 1;
    }
    meta = b[4];
    nt = meta & 15u;
    if (nt > 4u * 3u + ST_REST)
        return 0;
    memcpy(v1, b, 9);
    v1[4] = (uint8_t)(nt / 3u | (nt % 3u) << 3 | ((meta >> 4) & 3u) << 5 | (meta & 128u));   /* (as step_get9 reads it) */
    if (!step_get9(s, v1))
        return 0;
    for (k = 0; k < 9u; k++, bit++)
        c |= (uint32_t)((r[BK_PLANE + bit / 8u] >> (bit % 8u)) & 1u) << k;
    c |= ((meta >> 6) & 1u) << 9;
    if (!nm_uncode(s, c))
        return 0;
    nm_fit(s);
    return 1;
}

/* a pattern -> record r: steps st, LEN DIV SWING GATE DIR tv (the sound's own), the events of ms placed on track k, the
 * automation on or not: 0 for a step no format can hold (none the UI makes). ((PERNOTE) version 3 when a step holds a
 * note mode: bk_put10) */
static int bk_pack_raw(uint8_t *r, uint32_t k, const step_t *st, const int16_t *tv, const motion_store_t *ms, int on)
{
    uint32_t i, n = 0, np = PROJ_NP;
    /* ((Skiff phase 9) the motion's ids as a store of PROJ_NP has them, which firmware before reads; today's when it
     * names a parameter added since) */
    for (i = 0; i < ms->count && i < MOTION_MAX; i++)
        if ((ms->event[i].place >> 6) == k && MOTION_ID(&ms->event[i]) >= PROJ_NP - 8u && MOTION_ID(&ms->event[i]) < P_E0)
            np = P_COUNT;
    memset(r, 0, BK_REC);
    r[0] = BK_VER;
    r[1] = (uint8_t)np;
    for (i = 0; i < 5u; i++)
        r[2u + i] = (uint8_t)(tv[i] + 64);
    {
        uint32_t v3 = 0;                           /* ((PERNOTE) a note mode anywhere: version 3) */
        for (i = 0; i < NSTEP; i++) {
            step_t q = st[i];
            nm_fit(&q);
            v3 |= (uint32_t)nm_any(&q);
        }
        for (i = 0; i < NSTEP; i++) {
            step_t q = st[i];
            nm_fit(&q);
            if (!(v3 ? bk_put10(r, i, &q) : step_put9(r + BK_STEPS + i * 9u, &q)))
                return 0;
        }
        if (v3)
            r[0] = BK_VER3;
    }
    nudge_pack(r + BK_NUDGE, st, NSTEP);
    for (i = 0; i < ms->count && n < MOTION_MAX; i++) {
        const motion_event_t *e = &ms->event[i];
        uint8_t *m = r + BK_MOT + n * 4u;
        if ((e->place >> 6) != k)
            continue;
        uint32_t id = np == P_COUNT || MOTION_ID(e) < P_E0 ? MOTION_ID(e) : MOTION_ID(e) - PROJ_NX;
        m[0] = (uint8_t)((e->place & 63u) | (id >> 7) << 6);   /* ((1.3) the id's bit 7: BK_VER2) */
        m[1] = (uint8_t)((id & 127u) | (e->lock ? MOTION_LOCK : 0u));
        if (id > 127u && r[0] != BK_VER3)
            r[0] = BK_VER2;                        /* ((PERNOTE) version 3 holds such ids as 2 does) */
        m[2] = (uint8_t)e->value;
        m[3] = (uint8_t)((uint16_t)e->value >> 8);
        n++;
    }
    r[7] = (uint8_t)(n | (on ? 128u : 0u));
    return 1;
}
/* track t's pattern as it is (the sound's own timing, its automation and locks) -> record r */
static int bk_pack(uint8_t *r, const track_t *t)
{
    int16_t tv[5];
    uint32_t i;
    for (i = 0; i < 5u; i++)
        tv[i] = motion_base_value(t, BK_TIME[i]);
    return bk_pack_raw(r, trk_index(t), t->step, tv, &motion, motion_enabled(t));
}

/* chunk c as it is now -> bk_buf (never saved: all empty): 0, 2 a flash error */
static int bk_load(uint32_t c)
{
    bk_bufc = 0;
    if (!bk.sect[c]) {
        memset(bk_buf, 0, BK_LEN);
        return 0;
    }
    return bk_read_chunk(c) ? 2 : 0;               /* ((PERNOTE) an earlier build's chunk spread out) */
}
/* record rec -> pattern id (rec 0: the pattern emptied): 0 done, 2 not (the transport runs, a flash error), 3 no flash */
static int pool_put(uint32_t id, const uint8_t *rec)
{
    uint32_t c = bk_chunk(id);
    uint8_t *r = bk_buf + id % BK_PER * BK_REC;
    bk_bufc = 0;
    if (!BK_ON)
        return 3;
    if (transport_busy() || bk_load(c))
        return 2;
    if (rec)
        memcpy(r, rec, BK_REC);
    else
        memset(r, 0, BK_REC);
    if (bk_write(c))
        return 2;
    bk_lens(c);
    return 0;
}
static uint32_t pend_at(uint32_t id)                /* id's waiting copy, PEND_N none */
{
    uint32_t i;
    for (i = 0; i < PEND_N; i++)
        if (pend_id[i] == id % BK_POOL)
            return i;
    return PEND_N;
}
static uint32_t pend_free(void)                    /* the slots free */
{
    uint32_t i, n = 0;
    for (i = 0; i < PEND_N; i++)
        n += pend_id[i] == PAT_NONE;
    return n;
}
static int pool_used(uint32_t id)
{
    uint32_t i = pend_at(id);
    return i < PEND_N ? bk_ver(pend_rec[i][0]) : bk.len[id % BK_POOL] != 0u;
}
/* pattern id's record as it is now -> r: its waiting copy, else flash's: 0, 1 none (empty, or one no firmware wrote) */
static int pat_read(uint32_t id, uint8_t *r)
{
    uint32_t i = pend_at(id), c = bk_chunk(id);
    uint32_t rl = bk.leg[c] ? BK_REC1 : BK_REC;     /* ((PERNOTE) an earlier build's chunk: 888 apart, no plane) */
    if (i < PEND_N)
        memcpy(r, pend_rec[i], BK_REC);
    else if (!bk.len[id % BK_POOL] || !bk.sect[c] ||
             bk_read(bk_sector(bk.sect[c] - 1u) + 256u + id % BK_PER * rl, r, rl))
        return 1;
    else if (rl != BK_REC)
        memset(r + BK_REC1, 0, BK_REC - BK_REC1);
    return bk_ver(r[0]) && (r[7] & 127u) <= MOTION_MAX ? 0 : 1;
}
/* record r (all 0: empty it) to be pattern id: the same as it is now, nothing; else it waits (over id's waiting copy,
 * or in a free slot): 0 kept (or the same), 1 no room. Nothing is written here: pat_flush_one does, stopped */
static int pat_keep_rec(uint32_t id, const uint8_t *r)
{
    uint32_t i = pend_at(id);
    int now = pat_read(id, bk_buf);
    bk_bufc = 0;
    if (bk_ver(r[0]) ? !now && !memcmp(bk_buf, r, BK_REC) : now)
        return 0;                                  /* (the same, or empty and empty) */
    if (i == PEND_N)
        for (i = 0; i < PEND_N && pend_id[i] != PAT_NONE; i++)
            ;
    if (i == PEND_N)
        return 1;
    pend_id[i] = (uint8_t)(id % BK_POOL);
    memcpy(pend_rec[i], r, BK_REC);
    bk.gen++;
    return 0;
}
/* the waiting patterns of one chunk into flash, one write (the transport stopped; an erase mutes the audio): 0 done or
 * none waiting, 2 not (the transport runs, a flash error: they stay waiting) */
static int pat_flush_one(void)
{
    uint32_t i, c;
    for (i = 0; i < PEND_N && pend_id[i] == PAT_NONE; i++)
        ;
    if (i == PEND_N)
        return 0;
    c = bk_chunk(pend_id[i]);
    if (BK_ON) {                                   /* (no flash: nowhere to keep them, dropped) */
        if (transport_busy() || bk_load(c))
            return 2;
        for (i = 0; i < PEND_N; i++)
            if (pend_id[i] != PAT_NONE && bk_chunk(pend_id[i]) == c) {
                uint8_t *r = bk_buf + pend_id[i] % BK_PER * BK_REC;
                if (bk_ver(pend_rec[i][0]))
                    memcpy(r, pend_rec[i], BK_REC);
                else
                    memset(r, 0, BK_REC);
            }
        if (bk_write(c))
            return 2;
        bk_lens(c);
    }
    for (i = 0; i < PEND_N; i++)
        if (pend_id[i] != PAT_NONE && bk_chunk(pend_id[i]) == c)
            pend_id[i] = PAT_NONE;
    bk.gen++;
    return 0;
}
/* pat_keep_rec; no slot free and the transport stopped: a chunk written first to make room */
static int pat_keep_now(uint32_t id, const uint8_t *r)
{
    return pat_keep_rec(id, r) && (transport_busy() || pat_flush_one() || pat_keep_rec(id, r));
}
/* the autosave's round may wait for its gap (project.c): 0 nothing waiting, 1 some, 2 no slot free (not to wait) */
static uint32_t keep_waiting(void) { return pend_free() == PEND_N ? 0u : pend_free() ? 1u : 2u; }
/* the first empty id, none waiting either; NPAT: the pool full */
static uint32_t pool_alloc(void)
{
    uint32_t id;
    for (id = 0; id < BK_POOL; id++)
        if (!pool_used(id) && pend_at(id) == PEND_N)
            return id;
    return NPAT;
}
/* a pattern whose record is r (the LENs compared first); NPAT none */
static uint32_t pool_find(const uint8_t *r)
{
    uint32_t id, i;
    int32_t len = (int32_t)r[2] - 64;
    for (id = 0; id < BK_POOL; id++) {
        i = pend_at(id);
        if (!pool_used(id) || (i == PEND_N && bk.len[id] != (uint8_t)(len < 1 ? 1 : len > (int32_t)NSTEP ? NSTEP : len)))
            continue;
        if (!pat_read(id, bk_buf) && !memcmp(bk_buf, r, BK_REC)) {
            bk_bufc = 0;
            return id;
        }
    }
    bk_bufc = 0;
    return NPAT;
}

/* pattern id, read and checked, for track t: its steps, LEN DIV SWING GATE DIR (fitted to t's sound), its automation
 * and locks (t's places): 0 ok, 1 nothing there (or a record no firmware wrote) */
static int pat_decode(const track_t *t, uint32_t id, step_t *st, int16_t *v, motion_store_t *ms)
{
    uint8_t *r = bk_buf;
    uint32_t tk = trk_index(t), i, out;
    bk_bufc = 0;
    if (pat_read(id, r))                           /* (a waiting copy first) */
        return 1;
    memset(st, 0, NSTEP * sizeof *st);
    for (i = 0; i < NSTEP; i++)
        if (!bk_get10(&st[i], r, i))               /* ((PERNOTE) as its version stores it) */
            return 1;
    nudge_unpack(st, r + BK_NUDGE, NSTEP);
    for (i = 0; i < 5u; i++)
        v[i] = (int16_t)param_fit(track_desc(t, BK_TIME[i]), (int32_t)r[2u + i] - 64);
    memset(ms, 0, sizeof *ms);
    ms->count = (uint8_t)(r[7] & 127u);
    ms->on = (uint8_t)(r[7] & 128u ? 1u << tk : 0u);
    for (i = out = 0; i < ms->count && i < MOTION_MAX; i++) {
        const uint8_t *m = r + BK_MOT + i * 4u;
        int32_t val = (int16_t)(m[2] | (uint16_t)m[3] << 8);   /* (stored 16 bits: checked before it is a byte) */
        ms->event[i].place = (uint16_t)(tk << 6 | (m[0] & 63u));
        ms->event[i].id = (uint8_t)((m[1] & 127u) | (m[0] & 64u) << 1);   /* ((1.3) bit 7: BK_VER2's; 0 before) */
        ms->event[i].lock = m[1] >> 7;
        ms->event[i].value = (int8_t)val;
        out |= val < -64 || val > 127;
    }
    if (out || !proj_motion_ids(ms, r[1]) || !motion_valid(ms))
        ms->count = 0;                             /* (motion no firmware wrote: the steps still load) */
    return 0;
}

/* ---- (phase 5) the lists (ui.c lists): a track plays the pattern of the entry it is on, and what it has is kept as
 * that pattern (trk_pid); a new one gets the first free id when it is first kept. The (track, entry) functions keep
 * the bank's names: PERFORM's keys, SAVE > PATTERNS, the scenes ---- */
static uint32_t pool_len(uint32_t id)               /* pattern id's LEN, 0 empty (a waiting copy's first) */
{
    uint32_t i = pend_at(id);
    int32_t len;
    if (i == PEND_N)
        return bk.len[id % BK_POOL];
    if (!bk_ver(pend_rec[i][0]))
        return 0;
    len = (int32_t)pend_rec[i][2] - 64;
    return (uint32_t)(len < 1 ? 1 : len > (int32_t)NSTEP ? (int32_t)NSTEP : len);
}
static int bank_used(uint32_t k, uint32_t e) { uint32_t id = ent_pat(k, e); return id != PAT_NONE && pool_used(id); }
static void rec_motion_of(uint8_t *r, uint32_t id)  /* pattern id's stored automation into record r (motion_lost) */
{
    if (!pat_read(id, bk_buf)) {
        r[7] = bk_buf[7];
        memcpy(r + BK_MOT, bk_buf + BK_MOT, BK_REC1 - BK_MOT);   /* ((PERNOTE) the motion only, not the plane) */
    }
    bk_bufc = 0;
}
/* track k as a record (bk_rec): its steps, the sound's own timing, its automation (a pattern that came in without its
 * automation keeps its stored one: motion_lost). 0: a step no format holds */
static int trk_rec(uint32_t k)
{
    k %= NTRK;
    if (!bk_pack(bk_rec, &trk[k]))
        return 0;
    if (((motion_lost >> k) & 1u) && trk_pid[k])
        rec_motion_of(bk_rec, trk_pid[k] - 1u);
    return 1;
}
/* new pattern id (kept already) is track k's: named by the entry the track is on (that one naming another: the first
 * empty entry, the track on it; none: the pool keeps it, unnamed) */
static void trk_name(uint32_t k, uint32_t id)
{
    uint32_t e = lists.cur[k] % NENT;
    trk_pid[k] = (uint8_t)(id + 1u);
    if (lists.ent[k][e][0])
        for (e = 0; e < NENT && lists.ent[k][e][0]; e++)
            ;
    if (e < NENT) {
        lists.ent[k][e][0] = (uint8_t)(id + 1u);
        lists.cur[k] = (uint8_t)e;
    }
}
/* track k kept: its pattern, changed since it came in (trk_sig), waits to be written (pat_keep_rec); steps on no
 * pattern yet become a new one, the first free id (trk_name). 0 kept (or unchanged, or nothing there), 1 not: PATTERNS
 * FULL (no id, no waiting slot) */
static int trk_keep(uint32_t k)
{
    uint32_t h, id;
    k %= NTRK;
    if (!BK_ON)
        return 0;
    if (!trk_pid[k] && seq_is_empty(&trk[k]) && !motion_count(&trk[k]))
        return 0;                                  /* (nothing there: nothing to keep) */
    if (!trk_rec(k))
        return 1;
    h = proj_hash(bk_rec, BK_REC);
    if (trk_pid[k] && h == trk_sig[k])
        return 0;
    id = trk_pid[k] ? trk_pid[k] - 1u : pool_alloc();
    if (id == NPAT || pat_keep_now(id, bk_rec))
        return 1;
    if (!trk_pid[k])
        trk_name(k, id);
    trk_sig[k] = h;
    return 0;
}
/* stopped (a project save or load, a backup): every track kept, all of it written now: 0, 1 PATTERNS FULL, 2 a flash
 * error (what is not written still waits) */
static int keep_all(void)
{
    uint32_t k;
    int rc = 0;
    bank_poll();                                   /* (a swap the ISR made before the stop: what went out kept first) */
    for (k = 0; k < NTRK; k++)
        if (trk_keep(k))
            rc = 1;
    while (pend_free() < PEND_N)
        if (pat_flush_one())
            return 2;
    return rc;
}
/* the autosave's round (stopped and quiet: project.c autosave_poll): every track kept, one chunk of the waiting
 * patterns written (one erase a call): 0 nothing waits, 1 one written (more may wait), 2 a write failed */
static int keep_poll(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        (void)trk_keep(k);
    if (pend_free() == PEND_N)
        return 0;
    return pat_flush_one() ? 2 : 1;
}

/* pattern id into track t now (stopped: its steps, LEN DIV SWING GATE DIR, its automation and locks). No undo copy:
 * what the track had is kept. 0 loaded, 1 nothing there (or a record no firmware wrote), 2 loaded, its automation not
 * (AUTOMATION FULL: the pattern keeps it, motion_lost) */
static int pat_load(track_t *t, uint32_t id)
{
    static step_t st[NSTEP];
    static motion_store_t ms;
    uint32_t k = trk_index(t), i, full;
    int16_t v[5];
    if (pat_decode(t, id, st, v, &ms))
        return 1;
    motion_restore(t);
    fm1_irq_off();                                 /* the audio ISR must not see half a pattern */
    memcpy(t->step, st, sizeof st);
    for (i = 0; i < 5u; i++)
        t->p[BK_TIME[i]] = v[i];
    fm1_irq_on();
    full = motion_replace_track(t, &ms) != 0;
    if (full)
        motion_clear(t);                           /* (what it had goes with it, as at a launch: none) */
    motion_lost = (uint8_t)((motion_lost & ~(1u << k)) | (full ? 1u << k : 0u));
    motion_rebase(t);
    trk_pid[k] = (uint8_t)(id + 1u);
    trk_sig[k] = trk_rec(k) ? proj_hash(bk_rec, BK_REC) : 0u;
    bk.gen++;
    return full ? 2 : 0;
}
/* track k on its entry e now (stopped): what the track had kept, then the entry's pattern. 0, 1 an empty entry, 2
 * without its automation, 4 PATTERNS FULL (nowhere to keep what the track had) */
static int ent_go(uint32_t k, uint32_t e)
{
    uint32_t id = ent_pat(k, e);
    if (id == PAT_NONE || !pool_used(id))
        return 1;
    if (trk_keep(k))
        return 4;
    lists.cur[k % NTRK] = (uint8_t)(e % NENT);
    return pat_load(&trk[k % NTRK], id);
}

/* the page's mark: the entry track k is on, its pattern the track's (BK_NONE: none) */
static uint32_t bank_cur(uint32_t k)
{
    uint32_t e;
    k %= NTRK;
    e = lists.cur[k] % NENT;
    return lists.ent[k][e][0] && lists.ent[k][e][0] == trk_pid[k] ? e : BK_NONE;
}
/* how many entries name pattern id: the lists now (but track k's entry e), the saved projects' */
static uint32_t pat_refs(uint32_t id, uint32_t k, uint32_t e)
{
    uint32_t n = proj_slot_refs(id), t, j;
    for (t = 0; t < NTRK; t++)
        for (j = 0; j < NENT; j++)
            n += (t != k || j != e) && lists.ent[t][j][0] == id + 1u;
    return n;
}
/* SAVE > PATTERNS (stopped): track k's pattern into its entry e (that entry's pattern, else a new one; the track is
 * on it then); erase: the entry emptied, its pattern erased when nothing else names it; the track on that entry
 * emptied too (its steps and automation: else the next keep would make them a pattern there again). 0 done, 2 not
 * (the transport runs, a flash error), 3 no flash, 4 PATTERNS FULL */
static int bank_put(uint32_t k, uint32_t e, int erase)
{
    uint32_t id, j;
    k %= NTRK;
    e %= NENT;
    if (!BK_ON)
        return 3;
    if (transport_busy())
        return 2;
    bank_poll();                                   /* (a swap made before the stop: kept first) */
    id = ent_pat(k, e);
    if (erase) {
        if (id != PAT_NONE && !pat_refs(id, k, e)) {
            memset(bk_rec, 0, BK_REC);
            if (pat_keep_now(id, bk_rec))
                return 4;
            for (j = 0; j < NTRK; j++)
                if (trk_pid[j] == id + 1u && (j != k || lists.cur[k] != e)) {
                    trk_pid[j] = 0;                /* (on it unnamed: its steps a new pattern when next kept) */
                    motion_lost &= (uint8_t)~(1u << j);
                }
        }
        if (id != PAT_NONE && trk_pid[k] == id + 1u && lists.cur[k] == e) {
            if (pat_refs(id, k, e))
                (void)trk_keep(k);                 /* (the pattern stays: what the track had, kept into it first) */
            trk_pid[k] = 0;
            track_defaults_steps(&trk[k]);         /* (its automation too: motion_clear, motion_lost with it) */
        }
        lists.ent[k][e][0] = 0;
    } else {
        if (trk_pid[k] && trk_pid[k] != id + 1u && trk_keep(k))
            return 4;                              /* (the pattern it is on kept first) */
        if (id == PAT_NONE && (id = pool_alloc()) == NPAT)
            return 4;
        if (!trk_rec(k) || pat_keep_now(id, bk_rec))
            return 4;
        lists.ent[k][e][0] = (uint8_t)(id + 1u);
        lists.cur[k] = (uint8_t)e;
        trk_pid[k] = (uint8_t)(id + 1u);
        trk_sig[k] = proj_hash(bk_rec, BK_REC);
    }
    while (pend_free() < PEND_N)
        if (pat_flush_one())
            return 2;
    bk.gen++;
    return 0;
}

/* ---- live switching (launch.c): the main loop's side ---- */
static const char *const LAUNCH_AT[LAUNCH_N] = {" AT LOOP END", " AT NEXT BAR", " AT NEXT BEAT", " NEXT STEP"};
/* the launch_steps a BAR / BEAT launch staged now comes in on: the next bar's, beat's */
static uint32_t launch_at(uint32_t m)
{
    return m == LAUNCH_BAR ? (launch_steps / 16u + 1u) * 16u : (launch_steps / 4u + 1u) * 4u;
}
static uint32_t staged_n(void)                      /* the tracks with a pattern staged */
{
    uint32_t k, n = 0;
    for (k = 0; k < NTRK; k++)
        n += launch_q[k].armed != 0u;
    return n;
}
static int q_empty(const launch_t *q)               /* the pattern in q: no step sounds, no automation */
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (q->step[i].time == ST_NOTE && (q->step[i].n || q->step[i].hit))
            return 0;
    return !q->motion.count;
}
/* each frame (ui_input): a swap the ISR made (done): what played (now in launch_q) is kept as the pattern it was (a new
 * one: a new id, named by the entry it was on), and the track is on the entry that came in; a pattern or a mute still
 * staged when the transport stopped comes in now, as while stopped (a song's next not: PLAY stages it again; a
 * project load drops them: launch.c launch_drop) */
static void bank_poll_trk(uint32_t k)
{
    launch_t *q = &launch_q[k];
    track_t *t = &trk[k];
    uint32_t id;
    if (mute_arm[k] && !song.playing) {
        t->p[P_MUTE] = (int16_t)(mute_arm[k] - 1u);
        mute_arm[k] = 0;
    }
    if (q->armed && !song.playing) {
        q->armed = 0;
        if (!q->adv)
            (void)ent_go(k, q->slot);
        q->adv = 0;
        return;
    }
    if (!q->done)
        return;
    q->done = 0;
    q->adv = 0;
    if ((trk_pid[k] || !q_empty(q)) && bk_pack_raw(bk_rec, k, q->step, q->time, &q->motion, (q->motion.on >> k) & 1u)) {
        id = trk_pid[k] ? trk_pid[k] - 1u : pool_alloc();   /* (a new pattern went out: the first free id) */
        if (trk_pid[k] && ((motion_lost >> k) & 1u))
            rec_motion_of(bk_rec, id);
        if (trk_pid[k] && proj_hash(bk_rec, BK_REC) == trk_sig[k])
            ;                                      /* (unchanged since it came in) */
        else if (id == NPAT || pat_keep_rec(id, bk_rec))
            ui_way("PATTERNS FULL", WAY_PATTERNS);           /* (bank_switch stages only with room: never) */
        else if (!trk_pid[k])
            trk_name(k, id);
    }
    trk_pid[k] = (uint8_t)(q->id == PAT_NONE ? 0u : q->id + 1u);   /* ((1.2.1, #12) a rest: on no pattern) */
    trk_sig[k] = q_sig[k];
    lists.cur[k] = (uint8_t)(q->slot % NENT);
    motion_lost = (uint8_t)((motion_lost & ~(1u << k)) | (q->mlost ? 1u << k : 0u));
    bk.gen++;
    ui.force = 1;
}
static void bank_poll(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        bank_poll_trk(k);
}
/* playing, nothing staged for track k (q not armed): entry e staged, launch mode m; a song's (adv) after reps passes of
 * the one playing. 0 staged, 1 an empty entry, 2 .. without its automation (AUTOMATION FULL), 4 PATTERNS FULL (nowhere
 * to keep what the track has: each swap may need a waiting slot, a new pattern an id). (1.2.1, #12) The song's on an
 * empty entry (song_poll: a rest): silence, no automation, the track's own LEN DIV SWING GATE DIR, so the rest's REPS
 * count passes of the length the track has; on no pattern (trk_pid 0), so nothing is kept unless it was recorded into */
static int stage_entry(uint32_t k, uint32_t e, uint32_t m, uint32_t adv, uint32_t reps)
{
    track_t *t = &trk[k];
    launch_t *q = &launch_q[k];
    uint32_t id = ent_pat(k, e), full = 0, i;
    if (pend_free() <= staged_n() || (!trk_pid[k] && !seq_is_empty(t) && pool_alloc() == NPAT))
        return 4;
    if (adv && !bank_used(k, e)) {
        for (i = 0; i < NSTEP; i++)
            step_clear(&q->step[i]);
        for (i = 0; i < 5u; i++)
            q->time[i] = motion_base_value(t, BK_TIME[i]);
        memset(&q->motion, 0, sizeof q->motion);
        id = PAT_NONE;
    } else if (id == PAT_NONE || pat_decode(t, id, q->step, q->time, &q->motion))
        return 1;
    if (motion.count - motion_count(t) + q->motion.count > MOTION_MAX) {
        q->motion.count = 0;                       /* (no room: the steps come in, not the automation) */
        full = 1;
    }
    q_sig[k] = 0;                                  /* (the track's sig once it is in: as trk_keep packs it then) */
    if (bk_pack_raw(bk_rec, k, q->step, q->time, &q->motion, (q->motion.on >> k) & 1u)) {
        if (full)
            rec_motion_of(bk_rec, id);
        q_sig[k] = proj_hash(bk_rec, BK_REC);
    }
    q->slot = (uint8_t)e;
    q->id = (uint8_t)id;
    q->mlost = (uint8_t)full;
    q->mode = (uint8_t)m;
    q->at = launch_at(m);
    q->adv = (uint8_t)adv;
    q->reps = (uint8_t)reps;
    q->done = 0;
    RING_PUBLISH();
    q->armed = 1;
    bk.gen++;
    return full ? 2 : 0;
}
/* entry e of track k: stopped (a count-in too), at once (ent_go); playing, staged for its launch point (ui.c
 * launch_mode), one staged before replaced (an empty entry leaves it staged). 0 done or staged, 1 an empty entry,
 * 2 .. without its automation (AUTOMATION FULL), 4 PATTERNS FULL (stage_entry) */
static int bank_switch(uint32_t k, uint32_t e)
{
    launch_t *q;
    uint32_t id;
    k %= NTRK;
    e %= NENT;
    q = &launch_q[k];
    id = ent_pat(k, e);
    if (id == PAT_NONE || !pool_used(id))
        return 1;
    q->armed = 0;                                  /* (from here the ISR leaves it alone) */
    RING_PUBLISH();
    bank_poll();                                   /* (a swap just made: kept first) */
    if (!song.playing)
        return ent_go(k, e);
    return stage_entry(k, e, launch_mode(), 0, 0);
}
/* ---- (phase 5) the song (SONG on: lists.song): each track plays its list in turn, after the last the first; an entry's
 * repeats its passes (launch.c launch_due). (1.2.1, #12) An empty entry between a track's first and last pattern is a
 * rest (silence for its REPS passes of the track's LEN); the empty ones after its last end the list (were: every empty
 * entry passed over). Each track on its own: its next comes in at
 * its own pass end. PLAY goes on from the entry each track is on; GLO + PLAY from the first ones (song_top); a launch
 * (PERFORM, a scene) moves a track's place in it ---- */
/* each frame (ui_input, after bank_poll): SONG on and playing, each track's next entry staged for its pass end (one
 * there; a list of one entry: none), waiting while there is no room to keep what would go out; SONG off: a song's
 * staging dropped */
static void song_poll(void)
{
    uint32_t k, e, cur, first, last;
    for (k = 0; k < NTRK; k++) {
        launch_t *q = &launch_q[k];
        if (!lists.song) {
            if (q->armed && q->adv) {
                q->armed = 0;
                bk.gen++;
            }
            continue;
        }
        if (!song.playing || q->armed || q->done)
            continue;
        cur = lists.cur[k] % NENT;
        for (first = 0; first < NENT && !bank_used(k, first); first++)
            ;
        if (first == NENT)
            continue;
        for (last = NENT - 1u; !bank_used(k, last); last--)
            ;
        e = cur >= first && cur < last ? cur + 1u : first;   /* (the entries between: a pattern or a rest) */
        if (e != cur)
            (void)stage_entry(k, e, LAUNCH_LOOP, 1u, lists.ent[k][cur][1] + 1u);
    }
}
/* GLO + PLAY with SONG on: from the top, each track on its list's first entry (playing: stopped there and then, what
 * is staged dropped, a mute staged turned); then PLAY */
static void song_top(void)
{
    uint32_t k, e;
    transport_stop_now();
    for (k = 0; k < NTRK; k++)
        launch_q[k].armed = 0;
    bank_poll();                                   /* (a swap made: kept first; a mute staged: now) */
    for (k = 0; k < NTRK; k++) {
        for (e = 0; e < NENT && (ent_pat(k, e) == PAT_NONE || !pool_used(ent_pat(k, e))); e++)
            ;
        if (e < NENT)
            (void)ent_go(k, e);
    }
    transport_req = 1;
}
/* the entry staged for track k (BK_NONE: none) */
static uint32_t bank_next(uint32_t k) { return launch_q[k % NTRK].armed ? launch_q[k % NTRK].slot : BK_NONE; }
/* track k's staged pattern not to come in after all: 1 there was one */
static int bank_cancel(uint32_t k)
{
    int was = launch_q[k % NTRK].armed;
    launch_q[k % NTRK].armed = 0;
    bk.gen++;
    return was;
}
/* OCT- in the PERFORM layer: nothing staged comes in after all, no track's pattern, no mute: 1 there was some (a song's
 * next not: it is the song, staged again at once) */
static int stage_cancel(void)
{
    uint32_t k;
    int was = 0;
    for (k = 0; k < NTRK; k++) {
        if (launch_q[k].armed && !launch_q[k].adv)
            was |= bank_cancel(k);
        was |= mute_arm[k] != 0u;
        mute_arm[k] = 0;
    }
    bk.gen++;
    return was;
}
/* track k's MUTE to m (0, 1): stopped (a count-in too) at once; playing, staged for the launch point (ui.c launch_mode;
 * LOOP: that track's pass end), what was staged before dropped (as it is already: nothing staged) */
static void mute_to(uint32_t k, uint32_t m)
{
    uint32_t l = launch_mode();
    k %= NTRK;
    mute_arm[k] = 0;
    bk.gen++;
    if (!song.playing) {
        trk[k].p[P_MUTE] = (int16_t)m;
        return;
    }
    if ((trk[k].p[P_MUTE] != 0) == (m != 0u))
        return;
    mute_mode[k] = (uint8_t)l;
    mute_at[k] = launch_at(l);
    RING_PUBLISH();
    mute_arm[k] = (uint8_t)(m ? 2u : 1u);
}
/* the PERFORM layer's black key k (1..4): track k's MUTE the other way (mute_to); a second press before a staged one
 * lands takes it back. What happened, said (at once: nothing, as the GLO layer) */
static void mute_switch(uint32_t k)
{
    char b[24] = {'T', (char)('1' + k % NTRK), ' ', 0};
    k %= NTRK;
    if (mute_arm[k]) {
        str_cpy(b + 3, mute_arm[k] == 2u ? "MUTE" : "UNMUTE", sizeof b - 3u);
        mute_arm[k] = 0;
        bk.gen++;
        ui_say(b, " CANCELLED");
        return;
    }
    mute_to(k, !trk[k].p[P_MUTE]);
    if (mute_arm[k]) {
        str_cpy(b + 3, mute_arm[k] == 2u ? "MUTE" : "UNMUTE", sizeof b - 3u);
        ui_say(b, LAUNCH_AT[launch_mode()]);
    }
}

/* ---- scenes: the PERFORM layer's KEYS SCENE (KNOB 2) ----
 * A scene: the entry each track is on (A..P; none: that track left as it is) and the MUTEs. Recalled, each track's
 * pattern switches and its MUTE turns as the keys do: at once stopped, staged for the launch point playing (an entry a
 * track is on already: nothing staged for it). 16 of them, the project's (phase 5: ui.c lists.scene, SKF2; kept by the
 * autosave as the rest of it): 3 bytes a scene, 5 bits a track of T1..T4 (0 none, 1..16 the entry), then their four
 * MUTE bits; then 2 bytes, a bit for each scene kept; (phase 6) then 2 bytes a scene for T5 and T6: 5 bits each, their
 * MUTEs at bits 10 and 11, bit 15 set when the scene holds them (one stored with four tracks, SKF2's: T5 and T6 left as
 * they are). (Before phase 5 they were the settings', in favorites bytes no engine uses, factory[14][0] ..
 * factory[15][17]: proj_migrate frees them, once: (Skiff phase 7) row 14 is DRUM-X's favourites since, so SCENE_FREED
 * says it is done) */
#define SCENES 16u
#define SCENE_MEM(i) (lists.scene[(i)])
static uint8_t *scene_old(void) { return (uint8_t *)favorites.factory + 14u * 32u; }   /* (a pointer: the bytes */
#define SCENE_OLD(i) (scene_old()[(i)])                 /* run over the rows of factory[][]) */
#define SCENE_FREED (favorites.factory[15][24])        /* SCENE_FREED_V: the old scene bytes freed (a byte no engine uses) */
#define SCENE_FREED_V 0xA5u
_Static_assert(5u * SCENES + 2u == PROJ_SCENES && 3u * SCENES + 2u == PROJ_SC2, "the scenes' bytes");
static uint8_t scene_last = 0xFFu;                 /* the scene recalled or stored last (its key lit); 0xFF none */
static void scene_forget(void) { scene_last = 0xFFu; }   /* (a project load: its own scenes) */
static uint32_t scene_bits(uint32_t s)
{
    s %= SCENES;
    return SCENE_MEM(3u * s) | (uint32_t)SCENE_MEM(3u * s + 1u) << 8 | (uint32_t)SCENE_MEM(3u * s + 2u) << 16;
}
static int scene_used(uint32_t s) { return ((SCENE_MEM(48u) | (uint32_t)SCENE_MEM(49u) << 8) >> (s % SCENES)) & 1u; }
/* track k in scene s: its entry's code (0 none, 1..16) | its MUTE << 5; SCENE_NOT: a scene not holding it */
#define SCENE_NOT 64u
static uint32_t scene_trk(uint32_t s, uint32_t k)
{
    uint32_t b;
    s %= SCENES;
    if (k < 4u) {
        b = scene_bits(s);
        return ((b >> (5u * k)) & 31u) | ((b >> (20u + k)) & 1u) << 5;
    }
    b = SCENE_MEM(50u + 2u * s) | (uint32_t)SCENE_MEM(51u + 2u * s) << 8;
    return b >> 15 ? ((b >> (5u * (k - 4u))) & 31u) | ((b >> (6u + k)) & 1u) << 5 : SCENE_NOT;
}
/* track k's entry in scene s: 0..15; BK_NONE: none (or a code no firmware wrote) */
static uint32_t scene_slot(uint32_t s, uint32_t k)
{
    uint32_t c = scene_trk(s, k % NTRK) & 31u;
    return c && c <= BK_SLOTS ? c - 1u : BK_NONE;
}
static uint32_t scene_now(void) { return scene_last; }
/* the tracks as they are now into scene s: the entry each is on (bank_cur; none: on no entry's pattern), each MUTE */
static void scene_store(uint32_t s)
{
    uint32_t v = 0, h = 1u << 15, k, m;
    s %= SCENES;
    for (k = 0; k < NTRK; k++) {
        uint32_t c = bank_cur(k), mu = trk[k].p[P_MUTE] != 0;
        c = c < BK_SLOTS ? c + 1u : 0u;
        if (k < 4u)
            v |= c << (5u * k) | mu << (20u + k);
        else
            h |= c << (5u * (k - 4u)) | mu << (6u + k);
    }
    SCENE_MEM(3u * s) = (uint8_t)v;
    SCENE_MEM(3u * s + 1u) = (uint8_t)(v >> 8);
    SCENE_MEM(3u * s + 2u) = (uint8_t)(v >> 16);
    SCENE_MEM(50u + 2u * s) = (uint8_t)h;
    SCENE_MEM(51u + 2u * s) = (uint8_t)(h >> 8);
    m = SCENE_MEM(48u) | (uint32_t)SCENE_MEM(49u) << 8 | 1u << s;
    SCENE_MEM(48u) = (uint8_t)m;
    SCENE_MEM(49u) = (uint8_t)(m >> 8);
    scene_last = (uint8_t)s;
    bk.gen++;
}
/* scene s recalled: 0 done or staged, 1 empty, 2 .. a pattern without its automation (AUTOMATION FULL), 4 .. a track
 * that could not go (PATTERNS FULL: nowhere to keep what it has; its mute turns all the same) */
static int scene_recall(uint32_t s)
{
    uint32_t k;
    int rc = 0, r;
    s %= SCENES;
    if (!scene_used(s))
        return 1;
    for (k = 0; k < NTRK; k++) {
        uint32_t c = scene_slot(s, k), e = scene_trk(s, k);
        if (e == SCENE_NOT)
            continue;                              /* ((phase 6) T5, T6: a scene of before holds nothing of them) */
        if (c == BK_NONE)
            ;                                      /* (the track left as it is, what is staged for it too) */
        else if (c == bank_cur(k))
            (void)bank_cancel(k);                  /* (it plays that one already: nothing else to come) */
        else if ((r = bank_switch(k, c)) == 4 || (r == 2 && rc != 4))
            rc = r;
        mute_to(k, e >> 5);
    }
    scene_last = (uint8_t)s;
    bk.gen++;
    return rc;
}
/* scene s stored, said ((Skiff W3) the dialog's OCT+, the editor's PERFORM too) */
static void scene_saved(uint32_t s)
{
    char b[24] = "SCENE ";
    fmt_int(b + 6, (int32_t)(s % SCENES) + 1);
    scene_store(s);
    ui_say(b, " SAVED");
}
/* scene s recalled, said: scene_recall's rc ((Skiff W3) the editor's PERFORM too) */
static int scene_go(uint32_t s)
{
    char b[24] = "SCENE ";
    int staged = song.playing, rc;
    fmt_int(b + 6, (int32_t)(s % SCENES) + 1);
    rc = scene_recall(s);
    if (rc == 1)
        ui_say(b, " EMPTY");
    else if (staged)
        ui_say(b, LAUNCH_AT[launch_mode()]);
    else
        ui_say("LOADED ", b);
    str_cpy(ui.msg2, rc == 2 ? "AUTOMATION FULL" : rc == 4 ? "PATTERNS FULL" : "", sizeof ui.msg2);
    return rc;
}
/* a SCENE key: scene s recalled, or (SAVE held) stored; what happened, said */
static void scene_key(uint32_t s, int store)
{
    if (store && scene_used(s))                       /* ((Skiff U2) over a scene: the dialog first, no undo) */
        confirm_open(CF_STORE_SCENE, s % SCENES);
    else if (store)
        scene_saved(s);
    else
        (void)scene_go(s);
}
/* something staged: a track's pattern or MUTE still to come (a song's next not: it always is) */
static int stage_pending(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if ((launch_q[k].armed && !launch_q[k].adv) || mute_arm[k])
            return 1;
    return 0;
}

/* track k to its entry e (SONG's OCT+, PERFORM's keys: the selected track; (Skiff W3) the editor's LIST PLAY): at once
 * stopped, staged playing (LAUNCH says when it comes in); what happened, said (a load takes no undo copy: what the track
 * had is kept); bank_switch's rc */
static int entry_go(uint32_t k, uint32_t e)
{
    char b[8] = {'T', (char)('1' + k % NTRK), ' ', (char)('A' + e % NENT), 0};
    int staged = song.playing, rc = bank_switch(k, e);
    if (rc == 1) {
        ui_say(b, " EMPTY");
        return rc;
    }
    if (rc == 4) {
        ui_way("PATTERNS FULL", WAY_PATTERNS);
        return rc;
    }
    if (staged)
        ui_say(b, LAUNCH_AT[launch_mode()]);
    else
        ui_say(rc == 2 ? "AUTOMATION FULL " : "LOADED ", b);
    str_cpy(ui.msg2, staged && rc == 2 ? "AUTOMATION FULL" : "", sizeof ui.msg2);
    return rc;
}
static void entry_ui(uint32_t e) { (void)entry_go(song.sel % NTRK, e); }
/* (phase 5) SAVE > PATTERNS, the pool view: how many use pattern id: the entries naming it (the lists, the saved
 * projects), and a track on it that no entry of its names (0: it can be cleared) */
static uint32_t pool_refs(uint32_t id)
{
    uint32_t n, k;
    id %= BK_POOL;
    n = pat_refs(id, NTRK, 0);
    for (k = 0; k < NTRK; k++)
        n += trk_pid[k] == id + 1u && bank_cur(k) == BK_NONE;
    return n;
}
/* .. pattern id cleared (its dialog's OCT+: nothing uses it; stopped: an erase); said */
static void pool_clear(uint32_t id)
{
    char b[16], c[4];
    int rc = 0;
    id %= BK_POOL;
    pat_num(c, id);
    if (!BK_ON) {
        ui_message("STORAGE ERROR");
        return;
    }
    if (transport_busy() || pool_refs(id)) {
        ui_message(transport_busy() ? "STOP TO CLEAR" : "IN USE");
        return;
    }
    memset(bk_rec, 0, BK_REC);
    if (pat_keep_now(id, bk_rec))
        rc = 4;
    while (!rc && pend_free() < PEND_N)
        if (pat_flush_one())
            rc = 2;
    bk.gen++;
    ui.force = 1;
    if (rc) {
        ui_way(rc == 4 ? "PATTERNS FULL" : "CLEAR FAILED", rc == 4 ? WAY_PATTERNS : "");
        return;
    }
    str_cpy(b, "CLEARED ", sizeof b);
    str_cpy(b + 8, c, sizeof b - 8u);
    ui_message(b);
}
static uint32_t bank_gen(void) { return bk.gen; }

/* ---- (phase 5) SEQ > SONG (ui.c sg; ui_input.c sends OCT+ OCT- EDIT here): the selected track's entry picked ---- */
/* "T2 C" (b: 8 bytes) */
static void song_tag(char *b, uint32_t k, uint32_t e)
{
    b[0] = 'T';
    b[1] = (char)('1' + k % NTRK);
    b[2] = ' ';
    b[3] = (char)('A' + e % NENT);
    b[4] = 0;
}
/* OCT+: KNOB 2's pick into the entry (a pattern; the track on the entry: it comes in, at once stopped, at LAUNCH
 * playing), COPY: a new pattern, a copy of the entry's (the track's edits in it first), the entry's then (the track on
 * it: on the copy); no pick: the entry played (entry_ui). What happened, said */
/* (Skiff W3) .. into track k's entry e (the editor's LIST SET, COPY too): 0 done, else entry_go's rc (the track on the
 * entry), 1 NOTHING TO COPY, 4 PATTERNS FULL */
static int song_put(uint32_t k, uint32_t e, uint32_t p)
{
    uint32_t id = ent_pat(k, e), n;
    char b[24], c[4];
    if (p != SPICK_COPY) {
        lists.ent[k][e][0] = (uint8_t)p;           /* (p: the id + 1, as the entry keeps it) */
        bk.gen++;
        if (lists.cur[k] == e)
            return entry_go(k, e);
        song_tag(b, k, e);
        pat_num(c, p - 1u);
        str_cpy(b + 4, " SET ", sizeof b - 4u);
        str_cpy(b + 9, c, sizeof b - 9u);
        ui_message(b);
        return 0;
    }
    bank_poll();
    if (id == PAT_NONE || (trk_pid[k] == id + 1u && trk_keep(k)) || (n = pool_alloc()) == NPAT || pat_read(id, bk_rec) ||
        pat_keep_now(n, bk_rec)) {
        ui_way(id == PAT_NONE ? "NOTHING TO COPY" : "PATTERNS FULL", id == PAT_NONE ? "" : WAY_PATTERNS);
        return id == PAT_NONE ? 1 : 4;
    }
    lists.ent[k][e][0] = (uint8_t)(n + 1u);
    if (trk_pid[k] == id + 1u && lists.cur[k] == e)
        trk_pid[k] = (uint8_t)(n + 1u);            /* (the copy the track's from here: the same steps, the same sig) */
    bk.gen++;
    str_cpy(b, "COPIED ", sizeof b);
    pat_num(b + 7, id);
    str_cpy(b + 9, " TO ", sizeof b - 9u);
    pat_num(b + 13, n);
    ui_message(b);
    return 0;
}
static void song_oct(void)
{
    uint32_t p = song_pick();
    sg.pick = 0;
    if (!p)
        entry_ui(sg.entry % NENT);
    else
        (void)song_put(song.sel % NTRK, sg.entry % NENT, p);
}
/* OCT-: the pick dropped; none, what is staged for the track (a song's: no) */
static void song_back(void)
{
    uint32_t k = song.sel % NTRK;
    if (song_pick()) {
        sg.pick = 0;
        ui_message("CANCELLED");
    } else if (launch_q[k].armed && !launch_q[k].adv && bank_cancel(k)) {
        ui_message("CANCELLED");
    }
}
/* the CLEAR ENTRY dialog's lines for the selected track's entry e (l: 3 x 32 bytes): 1 .. 3 of them; *gone: its pattern
 * goes with it (the dialog: it can't be undone) */
static uint32_t song_clear_text(uint32_t e, char l[3][32], int *gone)
{
    uint32_t k = song.sel % NTRK, id = ent_pat(k, e), t, j, m = 0, n = 0, w[2] = {0, 0};
    char c[4];
    e %= NENT;
    pat_num(c, id);
    for (t = 0; t < NTRK; t++)                     /* (the other entries naming it: the first two) */
        for (j = 0; j < NENT; j++)
            if ((t != k || j != e) && lists.ent[t][j][0] == id + 1u && m++ < 2u)
                w[m - 1u] = t * 16u + j;
    *gone = id != PAT_NONE && !pat_refs(id, k, e);
    if (*gone) {
        str_cpy(l[n], "erases pattern ", 32);
        str_cpy(l[n] + 15, c, 17);
        str_cpy(l[n++] + 17, " too", 15);
    } else {
        str_cpy(l[n], "pattern ", 32);
        str_cpy(l[n] + 8, c, 24);
        str_cpy(l[n++] + 10, " stays:", 22);
        if (!m) {
            str_cpy(l[n++], "a saved project uses it", 32);
        } else if (m > 2u) {
            fmt_int(l[n], (int32_t)m);
            str_cpy(l[n] + str_len(l[n]), " entries use it", 32 - str_len(l[n]));
            n++;
        } else {                                   /* "entry D uses it", "entries D and T1 A use it" */
            char *p = l[n];
            str_cpy(p, m == 1u ? "entry " : "entries ", 32);
            for (j = 0; j < m; j++) {
                if (j)
                    str_cpy(p + str_len(p), " and ", 32 - str_len(p));
                if (w[j] / 16u != k) {
                    char tt[4] = {'T', (char)('1' + w[j] / 16u), ' ', 0};
                    str_cpy(p + str_len(p), tt, 32 - str_len(p));
                }
                c[0] = (char)('A' + w[j] % 16u);
                c[1] = 0;
                str_cpy(p + str_len(p), c, 32 - str_len(p));
            }
            str_cpy(p + str_len(p), m == 1u ? " uses it" : " use it", 32 - str_len(p));
            n++;
        }
    }
    if (trk_pid[k] == id + 1u && lists.cur[k] == e) {   /* (the user's choice: say the track empties) */
        str_cpy(l[n], "T1 goes empty", 32);
        l[n++][1] = (char)('1' + k);
    }
    return n;
}
/* the dialog's OCT+: the entry cleared (bank_put); said. (Skiff W3) track k's (the editor's LIST CLEAR): bank_put's rc */
static int song_clear_at(uint32_t k, uint32_t e)
{
    char b[8];
    int rc = bank_put(k, e, 1);
    song_tag(b, k, e);
    if (rc == 0)
        ui_say(b, " CLEARED");
    else if (rc == 3)
        ui_message("STORAGE ERROR");
    else if (rc == 4)
        ui_way("PATTERNS FULL", WAY_PATTERNS);
    else
        ui_message(transport_busy() ? "STOP TO CLEAR" : "CLEAR FAILED");
    ui.force = 1;
    return rc;
}
static void song_clear(uint32_t e) { (void)song_clear_at(song.sel % NTRK, e); }

/* ---- the editor's backup (editor_backup.c): each chunk an object, ids 16..31 ---- */
/* chunk c, read into bk_buf: *len BK_LEN, 0 when every pattern in it is empty (or never saved) */
static const uint8_t *bank_chunk(uint32_t c, uint32_t *len)
{
    uint32_t r;
    c %= BK_CHUNKS;
    *len = 0;
    if (!bk.sect[c])
        return bk_buf;
    if (bk_bufc != c + 1u) {
        bk_bufc = 0;
        if (bk_read_chunk(c))                      /* ((PERNOTE) an earlier build's spread out: a backup of today's size) */
            return bk_buf;
        bk_bufc = (uint8_t)(c + 1u);
    }
    for (r = 0; r < BK_PER; r++)
        if (bk_ver(bk_buf[r * BK_REC]))
            *len = BK_LEN;
    return bk_buf;
}
/* a chunk from a backup (len 0: all empty): each pattern empty or as some firmware with this record wrote it */
static int bank_chunk_ok(const uint8_t *b, uint32_t len)
{
    uint32_t r, i;
    step_t s;
    uint32_t rl = len == BK_LEN1 ? BK_REC1 : BK_REC;   /* ((PERNOTE) a backup of an earlier build: 888 apart) */
    if (!len)
        return 1;
    if (len != BK_LEN && len != BK_LEN1)
        return 0;
    for (r = 0; r < BK_PER; r++) {
        const uint8_t *p = b + r * rl;
        if (!p[0])
            continue;
        if (!bk_ver(p[0]) || p[1] < 8u || p[1] > P_COUNT || (p[7] & 127u) > MOTION_MAX ||
            (rl == BK_REC1 && p[0] == BK_VER3))
            return 0;                              /* ((PERNOTE) an earlier build's record is never version 3) */
        for (i = 0; i < NSTEP; i++)
            if (!bk_get10(&s, p, i))
                return 0;
    }
    return 1;
}
/* a backup's chunk c into the bank (len 0: its patterns emptied): 0 done (or nothing to do), 1 not written */
static int bank_restore(uint32_t c, const uint8_t *b, uint32_t len)
{
    uint32_t k;
    c %= BK_CHUNKS;
    bk_bufc = 0;
    if (!len && !bk.sect[c])
        return 0;                                  /* (empty here too) */
    if (!BK_ON)
        return 1;
    if (len) {
        memcpy(bk_buf, b, len);
        if (len == BK_LEN1)                        /* ((PERNOTE) a backup of an earlier build: spread out) */
            bk_spread(bk_buf);
    } else
        memset(bk_buf, 0, BK_LEN);
    if (bk_write(c))
        return 1;
    bk_lens(c);
    for (k = 0; k < PEND_N; k++)
        if (pend_id[k] != PAT_NONE && bk_chunk(pend_id[k]) == c)
            pend_id[k] = PAT_NONE;                 /* (the backup's patterns there now: not what waited) */
    for (k = 0; k < NTRK; k++)
        if (trk_pid[k] && bk_chunk(trk_pid[k] - 1u) == c) {
            trk_pid[k] = 0;                        /* (its steps: a new pattern when next kept) */
            motion_lost &= (uint8_t)~(1u << k);
        }
    return 0;
}

/* ---- (phase 5) the projects of before SKF2 (the four slots, the autosave) become SKF2 ((phase 6) SKF3 with six
 * tracks): each track's steps, timing and automation a pool pattern (one the same there already: that one; an
 * empty track: none), its entry A; their old scenes none (the settings' old scene bytes freed). After a clean boot
 * (main.c: never after a crash, a hang or a failed boot) and after a backup's restore (editor_backup.c). The patterns
 * are written before the project naming them: a power cut on the way leaves the record before, and the next time
 * finds those patterns again. The pool full: that project stays as it is (it loads as before, its steps its own). 1:
 * something written ---- */
#if FELUCCA_FLASH
static void fm1_wdt_feed(void);                    /* (fm1_sys.h; the host tests' own) */
#endif
static int proj_migrate(int allowed)
{
    int wrote = 0;
#if FELUCCA_FLASH
    project_t *p = &proj_scratch;
    uint32_t o, k, i, id, used;
    int16_t tv[5];
    int len;
    if (!allowed || !BK_ON)
        return 0;
    for (o = 0; o < 5u; o++) {
        uint32_t obj = o < 4u ? OBJ_PROJECT0 + o : OBJ_AUTOSAVE;
        fm1_wdt_feed();
        proj_wire_gen++;
        len = st_load(obj, &proj_wire, sizeof proj_wire);
        if (len <= 0 || proj_fmt_of(&proj_wire, len) || !proj_import(p, &proj_wire, len))
            continue;                              /* (none, SKF2 or SKF3 already, or no project) */
        proj_bound(p);
        proj_drums_to_part(p);                     /* (what a load converts, here first: a pattern's automation */
        proj_phys(p);                              /* comes as it is stored) */
        proj_fm4(p);
        proj_perc(p);
        for (k = 0; k < NTRK; k++) {
            for (i = used = 0; i < NSTEP; i++)
                used |= p->t[k].step[i].n | p->t[k].step[i].hit;
            for (i = 0; i < p->motion.count; i++)
                used |= (p->motion.event[i].place >> 6) == k;
            if (!used || p->t[k].preset == PROJ_DEF_SOUND)
                continue;                          /* (empty, or its steps cleared when it loads: no pattern) */
            for (i = 0; i < 5u; i++)
                tv[i] = p->t[k].p[BK_TIME[i]];
            if (!bk_pack_raw(bk_rec, k, p->t[k].step, tv, &p->motion, (p->motion.on >> k) & 1u))
                break;
            if ((id = pool_find(bk_rec)) == NPAT && ((id = pool_alloc()) == NPAT || pat_keep_now(id, bk_rec)))
                break;                             /* (the pool full) */
            p->ent[k][0][0] = (uint8_t)(id + 1u);
        }
        while (pend_free() < PEND_N)
            if (pat_flush_one())
                return wrote;                      /* (a flash error: the rest another time) */
        if (k < NTRK || !proj_pack2(&proj_wire, p, PROJ_SKF) || st_save(obj, &proj_wire, PROJ_SKF->size))
            continue;                              /* (this one as it is) */
        if (o < 4u)
            memcpy(&proj_slot[o], &proj_wire, sizeof proj_wire);
        wrote = 1;
    }
    if (SCENE_FREED != SCENE_FREED_V) {            /* (the settings' old scenes, once: their 50 bytes ((phase 6) the
                                                    * favorites after them stay); (phase 7) later, row 14's are DRUM-X's) */
        memset(scene_old(), 0, PROJ_SC2);
        SCENE_FREED = SCENE_FREED_V;
        settings_save();
    }
#else
    (void)allowed;
#endif
    return wrote;
}
