/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA user interface.
 * Flat: SURF cards and panels on the palette's background, no rules, one type family (Barlow Condensed, three sizes),
 * tracks named by their numbers 1..6 on a cushion (icons.c trk_icon); (Skiff) BLOCKS: ui_blocks.c. Four columns <-> KNOB 1..4. Rendering is lazy:
 * every element remembers what it last drew and is redrawn only on change. */
static int project_save(uint32_t slot);
static void panel_setup(void);
static void project_load(uint32_t slot);
static int project_used(uint32_t slot);
static int up_used(uint32_t k);              /* user presets: upreset.c */
static int up_load(uint32_t k);
static uint32_t up_count(void);
static uint32_t up_nth(uint32_t n);
static uint32_t up_rank(uint32_t slot);
static uint32_t up_engine(uint32_t k);
static void up_name(uint32_t k, char *b);
static void up_slot_label(char *b, uint32_t k);
static void up_ui(uint32_t op, uint32_t k);
static void up_auto_name(char *b, uint32_t e, uint32_t k);   /* naming (ui_name.c) */
static void up_ui_named(uint32_t op, uint32_t k, const char *name);
static int project_save_as(uint32_t slot, const char *name);
static int project_name(uint32_t slot, char *b);
static int project_rename(uint32_t slot, const char *name);
static int project_erase(uint32_t slot);   /* (1.2.1, #6) SAVE > PROJECT: DELETE, NEW (project.c) */
static void project_new(void);
static int step_page(uint32_t g) { return g == GR_ROLL || g == GR_TILES; }   /* (Skiff) STEP's tiles, ROLL: one page's ways */
#define BANK_SLOTS 16u                         /* Skiff: the pattern bank (bank.c): A..P a track */
static int bank_used(uint32_t k, uint32_t s);
static int keep_all(void);                       /* (Skiff phase 5) bank.c: keeping the tracks' patterns */
static int keep_poll(void);
static uint32_t keep_waiting(void);
static int pat_load(track_t *t, uint32_t id);
static int pool_used(uint32_t id);
static uint32_t pool_alloc(void);
static uint32_t bank_cur(uint32_t k);
static uint32_t bank_gen(void);
static void entry_ui(uint32_t e);
static uint32_t pool_len(uint32_t id);         /* (Skiff phase 5) SAVE > PATTERNS, the pool view (bank.c) */
static uint32_t pool_refs(uint32_t id);
static void pool_clear(uint32_t id);
static void bank_poll(void);                   /* (Skiff) live switching (bank.c, launch.c) */
static void song_poll(void);                   /* (Skiff phase 5) the song: each track's list in turn (bank.c) */
static void song_top(void);
static void song_oct(void);                    /* .. SEQ > SONG's OCT+ OCT- and its dialog (bank.c) */
static void song_back(void);
static void song_clear(uint32_t e);
static uint32_t song_clear_text(uint32_t e, char l[3][32], int *gone);
static uint32_t bank_next(uint32_t k);
static int bank_cancel(uint32_t k);
static int stage_cancel(void);
static void mute_switch(uint32_t k);
static int scene_used(uint32_t s);              /* (Skiff) scenes (bank.c) */
static uint32_t scene_slot(uint32_t s, uint32_t k);
static uint32_t scene_now(void);
static void scene_forget(void);
static void scene_key(uint32_t s, int store);
static void scene_saved(uint32_t s);
static void scene_store(uint32_t s);
static int stage_pending(void);
static void project_cur_name(char *b);
static uint32_t project_mark(void);   /* (Skiff U3, A2) project.c: the slot's letter ('A'..'D', 0 none) | changed << 8 */
static int project_dirty(void);
static int project_changed(void);
static uint32_t user_of(const track_t *t)    /* user preset slot its sound came from, UP_SLOTS = none */
{
    return t->user && up_used(t->user - 1u) ? t->user - 1u : UP_SLOTS;
}
static uint32_t up_gen;                      /* bumped on every user bank change (redraws) */
#include "favorites.c"
/* MENU's two-valued settings (ui_menu.c MENU_FLAGS), a bit each in a byte no engine uses (as ui_layer.c layer_seen):
 * saved with the settings; 0 in older ones = every setting's default (append-only: a new setting takes a new bit,
 * its default is 0) */
#define ui_prefs (favorites.factory[15][30])
#define PREF_LATCH 1u                          /* (MENU > FX LATCH ON, #40: (Skiff U2) retired, a locked FX layer latches;
                                                * the bit is left as saved) */
#define PREF_ANIM_OFF 2u                       /* MENU > ANIM OFF (#46): values snap (no rolling digits, no glide) */
#define PREF_ACCEL 4u                          /* MENU > KNOB ACCEL ON (#52): fast turns of wide values x2..x8 */
#define PREF_USB_FIXED 8u                      /* MENU > USB LEVEL FIXED: USB audio at the full level, MASTER after */
#define PREF_BPM_LOCK 16u                      /* MENU > BPM LOCK ON (#58): SELECT sets the tempo only with GLO held */
#define PREF_LARGE 32u                         /* MENU > LARGE ON (#15, Discussion #80): big knob labels and values
                                                * (ui_draw.c large_kind); clear in every older setting = OFF */
#define PREF_SERIAL_OFF 64u                    /* MENU > USB SERIAL OFF (#67): no serial console, the device enumerates as
                                                * audio + MIDI only (usb.c usb_cdc_switch); clear in every older setting = ON */
#define PREF_RESTORE_OFF 128u                  /* MENU > RESTORE LAST OFF (1.2, Discussion #130): no autosave written, none
                                                * restored at power-on (project.c); clear in every older setting = ON */
/* (Skiff U1) the settings' one-time changes done, in a byte no engine uses (0 in every older setting): 1 BPM LOCK
 * turned ON once on every unit, so SELECT turns pages (main.c) */
#define ui_set_rev (favorites.factory[15][23])
/* (Skiff U4, the review's A3; DESIGN.md rule 11) MENU > DISPLAY > HINTS OFF: the one line of hint (HOME's HOLD HOME:
 * SHIFT, the step pages' PRESETS: STEP and KEYS: STEPS, a layer's first-tap HOLD [X]: SHORTCUTS, (U5) SHIFT's map)
 * goes; an action page's OCT+ / OCT- line stays (what they do now). A byte no engine uses, 0 = ON */
#define ui_hints_off (favorites.factory[15][22])
/* (Skiff 1.3) MENU > SYSTEM > CPU 2 CORES (BETA): the second core from the next power-on (main.c, hal/fm1_cpu1.h), bit 0
 * of a settings byte bank.c freed ([15][5]: zero on every unit since phase 5, SCENE_FREED); clear: one core, as 1.2. It
 * clears itself when core 1 misreads or stops answering (main.c) */
#define CPU2_BYTE (favorites.factory[15][5])
#define CPU2_SET (favorites.factory[15][24] == 0xA5u && (CPU2_BYTE & 1u))
#define PREVIEW_SET (favorites.factory[15][24] == 0xA5u && (CPU2_BYTE & 2u))   /* ((1.3) MENU > STEP PREVIEW ON) */
/* (Skiff) MENU > SYSTEM > CPU CLOCK 320 MHZ: JieLi's rated clock instead of the boot stage's 360 (hal/fm1_clk.h), switched
 * at boot (main.c), bit 2 of CPU2_BYTE; clear: 360, as 1.3. clk320: what this boot did about it, kept across a soft reset
 * (which keeps the PLL), forgotten at power-on; CLK_WHY: FM1_CLK320_E*'s words (hal/fm1_clk320_probe.h) */
#define CLK320_SET (favorites.factory[15][24] == 0xA5u && (CPU2_BYTE & 4u))
enum { CLK_NONE, CLK_ARMED, CLK_ON, CLK_KEPT, CLK_REFUSED, CLK_TRIPPED };   /* (ARMED: switching; TRIPPED: a reset then, so
                                                        * no switch till power-off) */
#define CLK320_MAGIC 0x434C4B32u                       /* "CLK2" */
struct { uint32_t magic; uint8_t state, why; uint16_t loop_mhz; } clk320 __attribute__((section(".noinit")));
static const char *const CLK_WHY[11] = {"", "ALREADY 320", "SYS MODE", "PLL2 NOT 270", "PLL2 REF", "DIVIDERS",
                                        "SYS NOT ON PLL", "USB ON PLL2", "SDRAM", "CACHE B9", "VERIFY"};
#ifndef CLK_MHZ
#define CLK_MHZ() 360u                                 /* the core clock now (felucca.c: the PLL's); the host, the browser */
#endif
/* (Skiff U3, R8) the unit's CLOCK, MIDI IN and TUNE (MENU > CONTROL, AUDIO; GLOBAL's and SYSTEM's before, saved in every
 * project, so a project load could switch the clock or the routing without a word): settings bytes no engine uses
 * ([15][18..20], free since phase 5; 0 = INT, CH1-6, 0 ct, the defaults, in every older setting). song.g[] holds them
 * as before (its readers unchanged); a project still stores them (older firmware reads them) and a load leaves them
 * (project.c project_apply). The first boot with them (ui_set_rev 1) takes the session's own as the unit's */
#define ui_unit_g(i) (favorites.factory[15][18 + (i)])
/* (Skiff 1.3) MENU > MIDI > T1..T6 CHANNEL: each track's MIDI channel as its distance up from the track's own
 * (seq.c trk_midi_ch), settings bytes bank.c freed ([15][6..11]: 0 on every unit since phase 5 = channels 1..6) */
#define ui_mch(i) (favorites.factory[15][6 + (i)])
static const uint8_t *const mch_io = &ui_mch(0);
static const uint8_t UNIT_G[3] = {G_CLOCK, G_ROUTE, G_TUNE};
static uint8_t unit_g_adopt __attribute__((section(".pool")));   /* the first boot: a load takes them (main.c) */
static uint32_t unit_g_t __attribute__((section(".pool")));      /* fm1_ms | 1 of a change not saved yet */
static int unit_g_id(uint32_t id) { return id == G_CLOCK || id == G_ROUTE || id == G_TUNE; }
/* (Skiff phase 9) PRESETS' LIST as a type (core.h PT_*, preset_t tag): the factory sounds of that type only. A byte no
 * engine uses (0 in every older setting: none, so ALL or FAV, favorites.filter, as before; settings_persist.c keeps it
 * below PT_N). favorites.filter stays 0 / 1: firmware before refuses a backup of any other (editor_backup.c) */
#define ui_ptag (favorites.factory[15][21])
_Static_assert(PT_N == 8, "settings_persist.c keeps ui_ptag below 8");
/* MENU > AUDIO CLICK, CLICK LEVEL, COUNT-IN (1.1, Discussion #131; click.c, seq.c): two bits each in another byte no
 * engine uses, saved with the settings: bits 0-1 CLICK (OFF REC ON), 2-3 CLICK LEVEL (stored 0 MID, 1 LOW, 2 HIGH),
 * 4-5 COUNT-IN (OFF, 1 BAR, 2 BARS); 0 in older settings = OFF / MID / OFF, so nothing clicks or counts in that did
 * not; 3 (unknown) reads as 0 (settings_persist.c puts it back). Bit 6 (1.2): MENU > SCALE LEDS ON (Discussion #127,
 * ui_input.c scale_leds; 0 in older settings = OFF). Bit 7 (Skiff): FEEL ON, the REC layer's KNOB 4 (seq.c rec_feel;
 * 0 in older settings = OFF: REC and TAKES on the grid, as they were) */
#define ui_rec_prefs (favorites.factory[15][28])
#define PREF_REC 0xFF00u                       /* (menu_items.c MENU_FLAGS: a bit of ui_rec_prefs, << 8) */
#define PREF_SCALE_LEDS (0x40u << 8)
#define REC_FEEL 0x80u                         /* (Skiff: a bit of ui_rec_prefs itself) */
enum { RP_CLICK, RP_LEVEL, RP_COUNTIN };
static uint32_t rp_get(uint32_t f)                     /* a field as the MENU steps it: 0..2 */
{
    uint32_t v = (ui_rec_prefs >> (2u * f)) & 3u;
    v = v == 3u ? 0u : v;
    return f == RP_LEVEL ? (v == 0u ? 1u : v == 1u ? 0u : 2u) : v;   /* (LEVEL: LOW MID HIGH, MID stored as 0) */
}
static void rp_apply(void)                             /* the settings into click.c / seq.c (every frame: ui_input) */
{
    click_mode = (uint8_t)rp_get(RP_CLICK);
    click_lvl = (uint8_t)rp_get(RP_LEVEL);
    cin_bars = (uint8_t)rp_get(RP_COUNTIN);
    rec_feel = (ui_rec_prefs & REC_FEEL) != 0u;         /* (Skiff) */
}
static void rp_put(uint32_t f, uint32_t v)
{
    uint32_t st = f == RP_LEVEL ? (v == 1u ? 0u : v == 0u ? 1u : 2u) : v % 3u;
    ui_rec_prefs = (uint8_t)((ui_rec_prefs & ~(3u << (2u * f))) | st << (2u * f));
    rp_apply();
}
/* (favorites.factory[15][29] held MENU > STYLE, FLAT LINE BLOCKS: (Skiff U2) BLOCKS is the only look, the byte is left
 * as it was saved) */
/* (Skiff) the PERFORM layer's KNOB 1, LAUNCH: where a pattern switched to while playing comes in (launch.c LAUNCH_*:
 * LOOP BAR BEAT NOW), a byte no engine uses (favorites.c), saved with the settings; 0 in older settings = LOOP */
#define ui_launch (favorites.factory[15][26])
static uint32_t launch_mode(void) { return ui_launch < LAUNCH_N ? ui_launch : LAUNCH_LOOP; }
static const char *const LAUNCH_NAME[LAUNCH_N] = {"LOOP", "BAR", "BEAT", "NOW"};
/* (Skiff) the PERFORM layer's KNOB 2, KEYS: its white keys the selected track's patterns (0) or the scenes (1)
 * (bank.c), saved with the settings, as LAUNCH; 0 in older settings */
#define ui_perf_keys (favorites.factory[15][25])
static uint32_t perf_scenes(void) { return ui_perf_keys == 1u; }

static uint8_t sync_reload;                  /* engine / preset / project / user preset loaded: editor RELOAD push */

#define ACC T_THEME                /* values, curves */
#define VAL(c) ((c) == ui.hot_col && ui.hot_t ? T_ACCENT : T_THEME)   /* the knob just turned: accent */
#define RATIO(d, v) ((d)->max > (d)->min ? ((int32_t)(v) - (d)->min) * 1000 / ((d)->max - (d)->min) : -1)
/* layout: header 0..24, four cards 28..72 (57 px at x 3 + 59 c),
 * the panel 76..198 (a SURF area; the graphs live in it), footer 202..240; BG between them */
#define Y_HEAD 0
#define H_HEAD 24
#define Y_LABEL 28                    /* the cards */
#define Y_SEP_END 72
#define CARD_W 57
#define CARD_H 44
#define CARD_X(c) (3 + 59 * (int32_t)(c))
#define Y_GRAPH 76                    /* the panel */
#define H_GRAPH 122
#define Y_FOOT 202
#define H_FOOT 38
/* MENU > LARGE (large_kind below): on the value pages the cards are tall (28..132) and the panel a strip
 * (136..198); elsewhere (lists, rolls, layers) the layout above, its card labels in M */
#define LG_CARD_H 104
#define LG_Y_GRAPH 136
#define LG_H_GRAPH 62

static struct {
    uint8_t home;
    uint8_t home_scope;          /* Skiff: HOME's panel the scope (HOME tapped on HOME) instead of the tracks (ui_home.c) */
    uint8_t take_t;              /* Skiff: the TAKE box's frames left (ui_take.c; a message ends it) */
    uint8_t undo_w;              /* Skiff: the undo window's frames left (ui.c undo_step; a message ends it) */
    uint8_t page;                /* index into PAGES */
    uint8_t fam_last[FAM_COUNT]; /* last page used per family */
    uint8_t bank;                /* SEQ: 16-step bank (follows the cursor) */
    uint8_t cursor;              /* SEQ: step being edited (STEP page KNOB 1 moves it) */
    uint8_t entry_open;          /* SEQ: keys held since the first press of this entry */
    uint8_t lane;                /* SEQ > STEP on a DRUM track (the grid): the lane the keys and KNOB 3 / 4 edit */
    uint8_t npick;               /* (PERNOTE) SEQ > NOTES: the cursor step's note KNOB 2 picked (0..3, kept below its n) */
    uint8_t plk_src;             /* parameter locks (ui_input.c lock_*): the sound page whose KNOB 1..4 a step held
                                  * locks, PAGES index + 1; 0 = HOME's four */
    uint8_t plk_lane, plk_trk;   /* .. the grid: the lane and the track of the hits plk_rm takes away */
    uint64_t plk_rm, plk_acc;    /* .. the grid's step keys pressed on a hit (bit = step): it goes when the key is let
                                  * go, unless the key was held for a lock (its accent: plk_acc) */
    uint8_t hot_col, hot_t;      /* column whose knob was just turned (drawn white) */
    uint8_t menu;                /* 0 off, 1 list, 2 about + credits (HOME held) */
    uint8_t menu_sel;            /* MENU: the row (menu_items.c MI_*; its tab MI_TAB), kept while the device runs */
    uint8_t menu_row[5];         /* MENU: the row last picked in each tab, from its first (ALGORITHM comes back to it) */
    volatile uint8_t mpc[NTRK];  /* ((1.3) MIDI Program Change per track, 0x80 | program, 0 none: midi_control.c mpc_io
                                  * notes it, midi_pc_poll loads it) */
    uint16_t menu_scroll;        /* continuous ABOUT + CREDITS position, pixels */
    uint32_t menu_sig, home_t0;  /* HOME press time (btn_hold) */
    uint8_t force;               /* full redraw pending */
    uint8_t msg_t;               /* transient message frames */
    uint8_t bpm_t;               /* frames the BPM stays highlighted after a SELECT turn */
    uint8_t act;                 /* action pages: the column whose action OCT+ does, + 1; 0 = none (act_col) */
    uint8_t act2;                /* (Skiff U2) the picked card's second action: USER's KNOB 3 NAME (not ERASE) */
    uint8_t ev_del;              /* (Skiff U2) AUTO LIST: VALUE turned past its lowest, DELETE picked */
    uint8_t edge_t;              /* (Skiff U4, F7) SELECT at a family's end: the footer's n/m flashes this many frames */
    uint8_t shift;               /* (Skiff U5, A1) HOME held alone (SHIFT): its map of the page over the panel (ui_layer.c
                                  * draw_shift) */
    uint8_t ev_top;              /* (Skiff U3, R5) AUTOMATION: the list's top row, the page's own (PLAY, CLEAR) */
    uint32_t save_t0;            /* SAVE press time (btn_hold: held = UNDO) */
    uint8_t confirm;             /* the OCT- / OCT+ dialog: CF_*, 0 = none */
    uint8_t confirm_trk;         /* the track it clears, the slot it overwrites */
    uint8_t uslot;               /* SAVE > USER: the selected user preset slot */
    uint8_t bslot;               /* (Skiff phase 5) SAVE > PATTERNS: the pattern picked, 01 .. 64 (the pool view) */
    uint8_t ev_row;              /* SEQ > AUTO LIST (ui_input.c ev_*): the row selected (the track's count: + ADD LOCK) */
    uint8_t ev_step, ev_id;      /* .. + ADD LOCK: the step and the parameter a lock is added on */
    uint8_t uboot;               /* main.c: seconds left before UPDATE MODE (OCT- + OCT+ held), 0 = none */
    uint32_t ly_t0;              /* the layer button's press time | 1, LY_* bits (ui_layer.c layer_gesture) */
    uint8_t ly;                  /* the layer whose button is down (LAYER_*), 0 = none */
    uint8_t layer;               /* the layer whose map is shown (LAYER_*), 0 = none */
    uint8_t lock;                /* #83: the layer locked open by a double tap (LAYER_*), 0 = none (ui_layer.c) */
    uint16_t pg_down;            /* page buttons down (panel ids) that act when let go */
    uint32_t layer_sig;          /* drawn-state cache of the map */
    char msg[24];
    char msg2[24];               /* a second message, shown when the first is over */
    uint32_t enc_t[NE];
    /* drawn-state cache */
    char col[4][32];
    uint32_t graph_sig, head_sig, foot_sig, frame;
    /* rolling digits (ui_draw.c roll_*): the four card values and the header BPM */
    struct {
        char from[7];            /* the value rolling out; "" = idle */
        uint8_t t0;              /* (ui.frame) of the value's last change */
        int8_t dir;              /* +1: it went up (the old digit leaves upward), -1: down */
        uint8_t sig;             /* a card: what its value is of (label, unit, track, engine, palette) */
    } roll[5];
    int16_t roll_bpm;            /* the BPM last drawn */
    struct {                     /* (1.3) MIDI LEARN (midi_learn.c; here, not loose globals: the audio ISR's data keeps
                                  * its layout and its cost, tests/target_budget.py) */
        uint8_t on;              /* MIDI LEARN is on */
        uint8_t pick, trk, id;   /* a parameter picked (1): its track and P_* id */
        volatile uint8_t io[2];  /* midi_control.c ml_arm, ml_heard */
    } ml;
} ui;

enum { CF_NONE, CF_CLEAR_SEQ, CF_OVR_PROJ, CF_OVR_USER,
       CF_INIT_SOUND, CF_CLEAR_MOTION, CF_ERASE_USER,
       CF_ERASE_BANK, CF_CLEAR_ENTRY, CF_LOAD_PROJ, CF_STORE_SCENE, CF_RESET_SET, CF_ERASE_PROJ, CF_NEW_PROJ, CF_COUNT };   /* ui.confirm: TOOLS' clears; SAVE over a used
                                   * slot; USER ERASE; (Skiff phase 5) SAVE > PATTERNS: CLEAR an unused pattern; SONG:
                                   * CLEAR the entry; (Skiff U1) PROJECT LOAD over the session */
/* ((Skiff U2) here: bank.c opens one too) the OCT- / OCT+ dialog (ui_draw.c draws it); trk: the track or the slot it is about */
static void confirm_open(uint32_t kind, uint32_t trk)
{
    ui.confirm = (uint8_t)kind;
    ui.confirm_trk = (uint8_t)trk;
    ui.act = 0;
    ui.force = 1;
}
/* (Skiff phase 5) SEQ > SONG (ui_input.c song_*, ui_graph.c graph_song): the entry KNOB 1 picked (A..P), KNOB 2's pick
 * for it (a pattern id + 1, SPICK_COPY: a copy of the entry's; 0 none: OCT+ plays the entry) and where it was made (1 +
 * the track x 16 + the entry: another track or entry since, none) */
#define SPICK_COPY (NPAT + 1u)
#define SPICK_CLEAR (NPAT + 2u)                /* ((Skiff U2) the entry cleared: its dialog, was EDIT) */
/* (PERNOTE) SEQ > NOTES: the roll with a note of the cursor step picked (params.c: GR_ROLL as ROLL, its title says which) */
static int notes_page(const page_t *pg) { return pg->scope == SC_STEP && pg->graph == GR_ROLL && str_eq(pg->title, "NOTES"); }
/* (PERNOTE) the note NOTES has picked in step st: below its n (0 with none) */
static uint32_t npick_of(const step_t *st) { return st->n ? (ui.npick < st->n ? ui.npick : st->n - 1u) : 0u; }
static struct { uint8_t entry, pick, pick_at; } sg __attribute__((section(".pool")));

/* MENU > DISPLAY > SCREEN OFF (1.1.5): after a while with no input on the panel the screen and its backlight go off
 * (a picture held for hours can stay faintly on the panel); the sound, the sequencer, MIDI and USB go on, the LEDs
 * stay as they are. NEVER, 5, 15, 30 (the default), 60 minutes, kept in a byte no engine uses, saved with the
 * settings: stored value ^ 3, so 0 (every older setting) is 30 MIN; 5..7 unknown read as 30 MIN.
 * Input is the panel's only: a button or key held or pressed, a knob turned (not MIDI, the editor or MASTER, an
 * analogue pot). The press, key or turn that wakes it is swallowed: no note (seq.c kb_asleep), no button, no
 * value, until everything is let go and the knobs rest for SCR_EAT_MS (PLAY still stops the music: ui_input). A
 * dialog, the count-in, the UPDATE MODE countdown and (Skiff U1) the music playing keep it on (and wake it); the
 * crash screen, UPDATE and UBOOT wake it at once (lcd_wake_now).
 * Off: lcd_power(0) (backlight off, DISPOFF, SLPIN) and nothing is drawn. Waking, a frame at a time (never a wait in
 * the loop): 120 ms after the SLPIN, SLPOUT (lcd_power(1)); 120 ms after that one frame redraws everything
 * (ui.force), then DISPON and the backlight (lcd_power(2)) */
#define ui_scr (favorites.factory[15][27])
#define SCR_DEF 3u                                     /* 30 MIN */
#define SCR_EAT_MS 300u
static const uint8_t SCR_MIN[5] = {0, 5, 15, 30, 60};  /* NEVER, minutes */
enum { SCR_ON, SCR_OFF, SCR_WAKE, SCR_SLPOUT, SCR_SHOW };
static struct {
    uint8_t st;                  /* SCR_*: SCR_WAKE asked, SCR_SLPOUT sent, SCR_SHOW the frame that redraws */
    uint8_t eat;                 /* the wake gesture: panel input swallowed (ui_input) */
    uint32_t idle;               /* fm1_ms of the last panel input (or blocker) */
    uint32_t t;                  /* fm1_ms of the last panel command (SLPIN, SLPOUT) */
    uint32_t eat_t;              /* fm1_ms of the last swallowed knob detent */
} scrn;
static uint32_t scr_get(void)                          /* the MENU's value: 0 NEVER .. 4 60 MIN */
{
    uint32_t v = ui_scr ^ SCR_DEF;
    return v < NELEM(SCR_MIN) ? v : SCR_DEF;
}
static void scr_put(uint32_t v)                        /* (the time counts from the change: the editor's too) */
{
    ui_scr = (uint8_t)((v < NELEM(SCR_MIN) ? v : SCR_DEF) ^ SCR_DEF);
    scrn.idle = fm1_ms;
}
static void scr_wake(void)
{
    if (scrn.st == SCR_OFF)
        scrn.st = SCR_WAKE;
    scrn.eat = 1;
}
static void scr_wake_now(void)                         /* main.c: the crash screen, UPDATE, UBOOT */
{
    if (scrn.st != SCR_ON)
        lcd_wake_now();
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}
/* ui_input, every pass: 1 = the pass is swallowed (the screen dark or waking, or the wake gesture going on) */
static int scr_input(uint32_t pressed, uint32_t notes)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes, k;
    int32_t moved = 0;
    if (pressed | notes | held | panel_moved)
        scrn.idle = fm1_ms;
    panel_moved = 0;
    if (scrn.st == SCR_ON && !scrn.eat)
        return 0;
    for (k = 0; k < NE; k++)                           /* (every knob's turns go) */
        moved |= panel_enc(k);
    panel_moved = 0;
    if (moved)
        scrn.eat_t = scrn.idle = fm1_ms;
    if (pressed | notes | held | (uint32_t)moved)
        scr_wake();
    if (scrn.st != SCR_ON || held || (int32_t)(fm1_ms - scrn.eat_t) < (int32_t)SCR_EAT_MS)
        return 1;
    scrn.eat = 0;                                       /* all let go, the knobs at rest: input again */
    kb_asleep = 0;
    return 0;
}
/* ui_draw, every frame: 1 = draw it */
static int scr_frame(void)
{
    uint32_t lim = SCR_MIN[scr_get()] * 60000u;
    if (ui.confirm || ui.uboot || seq_counting() || song.playing) {   /* (never dark under these; (Skiff U1) nor
                                                                         * while the music plays) */
        scrn.idle = fm1_ms;
        if (scrn.st == SCR_OFF)
            scr_wake();
    }
    switch (scrn.st) {
    case SCR_ON:
        if (!lim || (int32_t)(fm1_ms - scrn.idle) < (int32_t)lim)   /* (signed: robust to a wrap) */
            return 1;
        lcd_power(0);
        scrn.st = SCR_OFF;
        scrn.t = fm1_ms;
        scrn.eat = 1;
        kb_asleep = 1;
        break;
    case SCR_WAKE:
        if (fm1_ms - scrn.t >= 120u) {
            lcd_power(1);
            scrn.st = SCR_SLPOUT;
            scrn.t = fm1_ms;
        }
        break;
    case SCR_SLPOUT:
        if (fm1_ms - scrn.t >= 120u) {
            scrn.st = SCR_SHOW;
            ui.force = 1;
            return 1;
        }
        break;
    case SCR_SHOW:
        return 1;
    }
    ui.msg_t = 0;                                      /* (dark: what would have shown is over; a clean page back) */
    ui.msg2[0] = 0;
    ui.hot_t = ui.bpm_t = 0;
    return 0;
}
static void scr_shown(void)                            /* after the frame: the redrawn page on */
{
    if (scrn.st != SCR_SHOW)
        return;
    lcd_power(2);
    scrn.st = SCR_ON;
    scrn.idle = fm1_ms;
}

static const page_t *page_over;   /* a quick layer's own four knobs (ui_layer.c), while it edits or draws them */
static const page_t *cur_page(void) { return page_over ? page_over : &PAGES[ui.page]; }

/* (FM6 LAB) a sheet: a page whose four knobs are a window over a table, SELECT moving it before the pages beside it
 * (DRUM-X's SOUND, FM6's OPERATOR and ALL OPS: ui_input.c SHEETS). sheet_of: the page's, or 0 */
typedef struct {
    uint8_t graph, eng, nwin, *win;              /* the page (GR_*), the engine it is for, its windows, the one shown */
    void (*knob)(uint32_t k, int32_t s);
    void (*cards)(void);
    uint32_t (*sig)(void);                       /* what the cards and the panel show (ui_graph.c graph_signature) */
    void (*draw)(const track_t *t, uint16_t c);  /* the panel, from its top */
    const char *(*title)(void);                  /* the header's */
    void (*pick)(int32_t s);                     /* PRESETS */
    const char *pj;                              /* PRESETS' job (SHIFT's map) */
} sheet_t;
static const sheet_t *sheet_of(uint32_t g);
/* SELECT on a sheet: its window down (d > 0) or up; 0 at its ends (SELECT goes on to the page there) */
static int sheet_rows(const sheet_t *sh, int32_t d)
{
    int32_t w = (int32_t)(*sh->win % sh->nwin) + (d > 0 ? 1 : -1);
    if (w < 0 || w >= (int32_t)sh->nwin)
        return 0;
    *sh->win = (uint8_t)w;
    ui.hot_t = 0;                                       /* (another knob's card: none lit) */
    return 1;
}

/* MENU > LARGE (#15, Discussion #80: what KNOB 1..4 do, in bigger type). Per page type:
 *   LK_TALL  HOME and the value pages (EDIT ENV LFO MOD FX SLICER SCL CHORD ARP VOICE PATTERN STEP's knobs, MIXER,
 *            GLOBAL, SYSTEM, TOOLS, MOTION): tall cards (a K1..K4 keycap, the icon, the label in M, the value in L,
 *            in M or S when L is too wide or lacks a glyph, the unit under it, the gauge), the panel a strip: HOME's
 *            scope, ENV's ADSR, LFO's wave, PATTERN's 64 steps, MIXER's four tracks (name, state, meter) small; the
 *            page's title and number in L on the others (their charts cannot be read that small);
 *   LK_LABEL the list and graph pages (PRESETS, USER, PROJECT, PATTERNS, SONG, the piano roll and the drum grid,
 *            CHANCE, SLICES) and the quick layers' maps: the layout as it is, the card labels in M;
 *   LK_OFF   LARGE off; the menu (its own LARGE: ui_menu.c), the dialogs and NAME keep their own layout in every case. */
enum { LK_OFF, LK_LABEL, LK_TALL };
static uint32_t large_kind(void)
{
    uint32_t g;
    if (!(ui_prefs & PREF_LARGE))
        return LK_OFF;
    if (ui.layer)
        return LK_LABEL;
    if (ui.home)
        return LK_TALL;
    g = cur_page()->graph;
    return g == GR_BROWSE || g == GR_SLOTS || g == GR_USER || g == GR_BANK || step_page(g) ||
           g == GR_CHANCE || g == GR_PLAY || g == GR_SLICES || g == GR_EVENTS || g == GR_SONG || sheet_of(g) ? LK_LABEL
                                                                                                           : LK_TALL;
}
/* the geometry of the page shown: the cards' height, the panel's top and height */
static uint32_t card_h(void) { return large_kind() == LK_TALL ? LG_CARD_H : CARD_H; }
static uint32_t graph_y(void) { return large_kind() == LK_TALL ? LG_Y_GRAPH : Y_GRAPH; }
/* (Skiff) BLOCKS: the footer is one line, as the B mockup's (no step strip): its canvas 16 px lower, the panel 16 px
 * taller (ADSR and LFO grow into it, the fixed layouts sit centred: graph_pad) */
#define FOOT_LINE 16u
static uint32_t foot_less(void) { return FOOT_LINE; }
static uint32_t graph_h(void) { return (large_kind() == LK_TALL ? LG_H_GRAPH : H_GRAPH) + foot_less(); }
static int32_t graph_pad(void) { return (int32_t)foot_less() / 2; }
static uint32_t foot_y(void) { return Y_FOOT + foot_less(); }
static uint32_t foot_h(void) { return H_FOOT - foot_less(); }
static int32_t foot_line_y(void) { return 19 - (int32_t)foot_less(); }   /* the names' line in the footer's canvas */
static int large_face_has(const char *s)                /* every glyph of s in the sparse L face */
{
    for (; *s; s++)
        if (glyph_at(&AF_L, fold(&AF_L, (uint8_t)*s)) < 0)
            return 0;
    return 1;
}
static int32_t ink_w(const aafont_t *f, const char *s)
{
    int32_t b[4];
    text_ink(f, s, b);
    return b[2] > b[0] ? b[2] - b[0] : 0;
}

/* the quick layers (ui_layer.c): a button held, the keys and KNOB 1..4 are its shortcuts, its map over the page */
enum { LAYER_NONE, LAYER_FX, LAYER_GLO, LAYER_SCL, LAYER_EDIT, LAYER_SEQ, LAYER_REC, LAYER_PERF, LAYER_N };   /* (Skiff:
                                   * PERF, ARP held: the bank's patterns live, ui_layer.c) */
static void draw_layer(void);
static void draw_shift(void);
static const char *layer_head(void);
static int layer_locked(void);
static uint32_t layer_leds(uint32_t *br);
static uint32_t layer_btn(void);

/* (Skiff phase 7, 8) EDIT > SOUND, the DRUM-X sound sheet (ui_dx.c, included by ui_input.c): the sound edited, the
 * sheet's window the knobs are on (0..3) */
static uint8_t dx_ui_lane, dx_ui_win;
/* (FM6 LAB) EDIT > OPERATOR and ALL OPS (ui_fm6.c): the operator picked (0..5: OP1..OP6), OPERATOR's window, ALL
 * OPS' window (.pool: off the audio ISR's merged globals) */
static uint8_t fm_ui[3] __attribute__((section(".pool")));

/* FM operator pages belong to DIGITAL; they never appear on other instruments (without FELUCCA_FM4: never). SLICES:
 * a SLICE track's (ui_slice.c); (Skiff U3, R6) LANES a DRUM track's (its lane levels; DRUM-X's sheet has them);
 * (phase 7, 8) SOUND, the sheet, a DRUM-X track's (ui_dx.c). (Skiff U3, N5) a page that changes nothing on the track's
 * engine is not there: on the hits (engine_t.oneshot, DRUM and DRUM-X) ENV, ENV DEST, VOICE, VOICE 2 and SCL (its scale
 * and chords); where the operators are the envelope (ownenv, FM6) ENV and ENV DEST. (N2) ROLL is a melodic track's: on
 * a drum track STEP is its grid */
static int page_visible(uint32_t i)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    const sheet_t *sh = sheet_of(PAGES[i].graph);
    uint32_t id = PAGES[i].id[0];
    if (sh)
        return TSEL->eng_req % NENGINES == sh->eng;
    if (PAGES[i].graph == GR_LANES)
        return TSEL->eng_req % NENGINES == ENGI_DRUM;
    if (PAGES[i].graph == GR_ROLL)
        return !drum_track(TSEL);
    if (e->oneshot && (PAGES[i].fam == FAM_SCL || id == P_ATK || id == P_ED_FLT || id == P_EG2_ATK || id == P_VOICE ||
                       id == P_ALLOC))
        return 0;
    if (e->ownenv && (id == P_ATK || id == P_ED_FLT || id == P_EG2_ATK))   /* ((Skiff phase 9) ENV 2: where ENV is) */
        return 0;
#if FELUCCA_SLICE
    if (PAGES[i].graph == GR_SLICES)
        return ENGINES[TSEL->eng_req % NENGINES] == &ENG_SLICE;
#else
    if (PAGES[i].graph == GR_SLICES)
        return 0;
#endif
    return !(PAGES[i].fam == FAM_EDIT && PAGES[i].id[0] >= P_FM1_ATK &&
             PAGES[i].id[0] <= P_FM4_LEVEL) || (FELUCCA_FM4 && TSEL->eng_req % NENGINES == ENGI_DIGITAL);
}

/* every frame: song.g -> the bytes when they changed (the MENU, the editor), saved a second after the last change (the
 * MENU: when it closes, menu_close) */
static void unit_g_poll(void)
{
    uint32_t i;
    for (i = 0; i < 3u; i++)
        if (ui_unit_g(i) != (uint8_t)song.g[UNIT_G[i]]) {
            ui_unit_g(i) = (uint8_t)song.g[UNIT_G[i]];
            unit_g_t = fm1_ms | 1u;
        }
    if (unit_g_t && !ui.menu && fm1_ms - unit_g_t >= 1000u) {
        unit_g_t = 0;
        settings_save();
    }
}
/* every frame: PLAY with CLOCK USB or TRS follows that clock's pulses (seq.c events_block, midi_clock.c), so with none
 * a second after the start the music stands still: say so, once a start, and where CLOCK is. (A comment on the Learn
 * videos, 2026-10-19: PLAY and REC did nothing after the install till RESET SETTINGS; Skiff's first boot takes CLOCK
 * from the session it restores, unit_g_boot) */
static void ui_way(const char *s, const char *way);
static void clock_wait_poll(void)
{
    static uint8_t said;
    if (!song.playing || !song.g[G_CLOCK] || midi_clock.have_pulse)
        said = 0;
    else if (!said && fm1_ms - midi_clock.start_ms >= 1000u) {
        said = 1;
        ui_way(song.g[G_CLOCK] == 1 ? "WAITING FOR USB CLOCK" : "WAITING FOR TRS CLOCK", "MENU > MIDI > CLOCK");
    }
}
/* after the session came back (main.c, autosave_boot): the unit's into song.g; the first boot the other way */
static void unit_g_boot(void)
{
    uint32_t i;
    for (i = 0; i < 3u; i++)
        if (unit_g_adopt)
            ui_unit_g(i) = (uint8_t)song.g[UNIT_G[i]];
        else
            song.g[UNIT_G[i]] = (int16_t)clamp((int8_t)ui_unit_g(i), GP[UNIT_G[i]].min, GP[UNIT_G[i]].max);
    if (unit_g_adopt) {
        unit_g_adopt = 0;
        ui_set_rev = 2;
        settings_save();
    }
}

static uint32_t page_first(uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == fam)
            return i;
    return 0;
}
/* (Skiff U3, N3, N5) a family's first page the track has; NPAGES: none */
static uint32_t page_vis_first(uint32_t fam)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == fam && page_visible(i))
            return i;
    return NPAGES;
}
static uint32_t page_lane(void) { return ui.lane % 8u; }   /* (params.c page_desc: LANES' LEVEL) */

/* transient message in the top bar, right of the transport and the BPM: a + b, for MSG_T frames (a second one,
 * ui.msg2, as long after it). (Skiff U1) 90 frames, about 1.5 s: 40 (0.6 s) went by unread while playing */
#define MSG_T 90u
static void ui_say(const char *a, const char *b)
{
    uint32_t n;
    str_cpy(ui.msg, a, sizeof ui.msg);
    n = str_len(ui.msg);
    str_cpy(ui.msg + n, b, sizeof ui.msg - n);
    ui.msg_t = MSG_T;
    ui.msg2[0] = 0;
    ui.take_t = 0;                                     /* (Skiff: a newer word ends the TAKE box: an undo, a notice) */
    ui.undo_w = 0;                                     /* (.. and the undo window: OCT- / OCT+ the octave again) */
}

static void ui_message(const char *s) { ui_say(s, ""); }
/* (Skiff U4, the review's F4) MIDI IN into a track not selected: when (fm1_ms | 1; 0 never), HOME's lanes mark its
 * number for MIDI_MARK_MS (was a header message every 4 s per track, covering the title while a DAW played) */
#define MIDI_MARK_MS 400u
static uint32_t midi_at[NTRK] __attribute__((section(".pool")));
static int midi_mark(uint32_t k) { return midi_at[k % NTRK] && (fm1_ms | 1u) - midi_at[k % NTRK] < MIDI_MARK_MS; }
/* (Skiff U4, the review's F3) a refusal and, after it, the way out: the header's second message (ui.msg2) */
static void ui_way(const char *s, const char *way)
{
    ui_say(s, "");
    str_cpy(ui.msg2, way, sizeof ui.msg2);
}
#define WAY_PATTERNS "SAVE > PATTERNS: CLEAR"           /* PATTERNS FULL: free a number */
#define WAY_MOTION "AUTOMATION: DELETE ONE"           /* AUTOMATION FULL: free a record */
/* a message led by an icon: its first byte (ui_draw.c draw_head draws the icon, then the words) */
#define MSG_NOFILE "\x01"                      /* the no-file icon (ICON_X_NOFILE) */
/* a missing sample (ui_input.c sample_notice): NO SAMPLE (SAMPLE NOT FOUND, the SLICES page's, is 4 px too wide
 * for the header's 130 px after the icon) */
#define MSG_NO_SAMPLE MSG_NOFILE "NO SAMPLE"

/* Main loop only, with interrupts enabled. PLAY may be consumed between reads;
 * take one coherent snapshot before a flash operation. */
static int transport_busy(void)
{
    int busy;
    fm1_irq_off();
    busy = song.playing || transport_req == 1u || seq_counting();   /* (a count-in: as playing) */
    fm1_irq_on();
    return busy;
}
/* (Skiff) the transport stopped from the main loop there and then, as the audio ISR stops it (a count-in too) */
static void transport_stop_now(void)
{
    transport_req = 2;
    fm1_irq_off();
    seq_stop();
    transport_req = 0;
    fm1_irq_on();
}


/* parameter locks: a page whose KNOB 1..4 can lock (a track's sound: one of its parameters can be motion; not MOD's
 * slots), the parameter KNOB k locks (ui.plk_src's page, HOME's four), 0xFF = none */
static int lock_page_ok(uint32_t i)
{
    uint32_t k;
    if (i >= NPAGES || (PAGES[i].scope != SC_TRACK && PAGES[i].scope != SC_ENGINE) || PAGES[i].graph == GR_MOD)
        return 0;
    if (PAGES[i].graph == GR_LANES)                     /* ((Skiff U3) LANES: KNOB 2 the lane's level, (phase 9) KNOB 3
                                                         * 4 its TUNE DECAY, lock_id) */
        return 1;
    for (k = 0; k < 4u; k++)
        if (PAGES[i].id[k] != 0xFFu && motion_param(PAGES[i].id[k]))
            return 1;
    return 0;
}
static uint32_t lock_id(uint32_t k)
{
    uint32_t id = !ui.plk_src || !lock_page_ok(ui.plk_src - 1u) ? ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u] :
                  PAGES[ui.plk_src - 1u].graph == GR_LANES ? ((k & 3u) == 1u ? P_LN0 + page_lane() :
                                                              (k & 3u) == 2u ? P_LTUNE0 + page_lane() :
                                                              (k & 3u) == 3u ? P_LDECAY0 + page_lane() : 0xFFu) :
                  PAGES[ui.plk_src - 1u].id[k & 3u];
    return id < P_COUNT && motion_param(id) ? id : 0xFFu;
}

static void ev_enter(void);
static void page_entered(void)
{
    const page_t *pg = cur_page();
    if (!ui.home && pg->graph == GR_EVENTS)      /* AUTOMATION: + ADD LOCK on the SEQ cursor's step */
        ev_enter();
    if (!ui.home && lock_page_ok(ui.page))
        ui.plk_src = (uint8_t)(ui.page + 1u);
    song.seq_mode = !ui.home && pg->fam == FAM_SEQ;
    ui.entry_open = 0;
    ui.hot_t = 0;                                /* clear the previous page's emphasis */
    ui.act = pg->graph == GR_USER ? 4u : 0u;            /* the save screen: ready for OCT+ ((Skiff) PATTERNS: act_col) */
    ui.act2 = ui.ev_del = ui.ev_top = 0;
    ui.force = 1;
}

static int step_on(const step_t *st) { return st->time == ST_NOTE && (st->n || st->hit); }

static void step_clear(step_t *st)
{
    st->n = 0;
    st->time = ST_REST;
    st->flags = 0;
    st->vel = 0;
    st->hit = st->acc = 0;
    st->probability = 0;
    st->cond = 0;                                /* (Skiff: its trig condition too, its NUDGE) */
    st->nudge = 0;
    nm_clear(st);                                /* ((PERNOTE) its notes' own modes) */
}

/* ------------------------------------------------------- the DRUM grid --- */
/* SEQ > STEP on a DRUM track is the grid: 8 lanes x the 16 steps of a page. The white keys are the
 * steps of the page shown (a tap toggles the selected lane there), black keys 1..8 select the lane (and play
 * it), black key 9 held is ACC (white keys toggle accents, their LEDs show them), black keys 10 / 11 the page
 * down / up. KNOB 1 STEP, 2 LANE, 3 HIT, 4 ACC edit the cursor step. A sound load never converts the
 * steps: the grid shows a step's notes on their lanes (eng_drum.c step_lanes) and an edit makes the lane its
 * own (grid_own). Live recording on a DRUM track writes hits (seq.c rec_note) */
static int grid_on(void) { return !ui.home && !ui.menu && !ui.confirm && step_page(cur_page()->graph) && drum_track(TSEL); }   /* (STEP only: CHANCE is SC_STEP too) */

/* black key place p (seq.c key_place) held, 0 = not */
static int black_held(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) == p)
            return (int)(((fm1_in.notes & ~kb_layer) >> k) & 1u);
    return 0;
}

/* lane l of step s from now on is its hit alone: the step's lane notes become hits (nothing sounds different),
 * a note of another pitch on the lane (a low tom 41) becomes the lane's own */
static void grid_own(step_t *s, uint32_t l)
{
    uint32_t k, j = 0;
    step_to_grid(s);
    for (k = 0; k < s->n; k++)
        if (drum_lane(s->note[k]) == l)
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[j++] = s->note[k];
    for (k = j; k < 4u; k++)
        s->note[k] = 0;
    s->n = (uint8_t)j;
    nm_clear(s);                                 /* ((PERNOTE) a drum kit's notes have no TIE or SLD; its notes moved) */
}

/* lane l of step i: on 1, off 0, toggled 2 */
static void grid_hit(track_t *t, uint32_t i, uint32_t l, uint32_t on)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t b = 1u << (l % NLANE);
    if (on == 2u)
        on = !(step_lanes(s) & b);
    if (s->time != ST_NOTE) {                    /* a REST or a TIE: an empty step (nothing to turn off) */
        if (!on)
            return;
        step_clear(s);
        s->time = ST_NOTE;
    }
    grid_own(s, l % NLANE);
    if (on) {
        s->hit |= (uint8_t)b;
    } else {
        s->hit &= (uint8_t)~b;
        s->acc &= (uint8_t)~b;
        if (!s->n && !s->hit)
            step_clear(s);
    }
}

/* the accent of lane l at step i (on 1, off 0, toggled 2); an accent on an empty lane adds the hit */
static void grid_acc(track_t *t, uint32_t i, uint32_t l, uint32_t on)
{
    step_t *s = &t->step[i % NSTEP];
    uint32_t b = 1u << (l % NLANE);
    if (on == 2u)
        on = !(step_accents(s) & b);
    if (on)
        grid_hit(t, i, l, 1);
    if (!(step_lanes(s) & b))
        return;
    grid_own(s, l % NLANE);
    if (s->flags & SF_ACCENT) {                  /* a step accent: each hit's own from now on */
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
    s->acc = (uint8_t)(on ? s->acc | b : s->acc & ~b);
}

/* (Skiff 1.3, from Felucca 1.6 #242) SEQ's step pages (STEP, ROLL, CHANCE, PLAY) follow the play position: a melodic
 * pattern longer than the 16 steps on the screen shows the bank the playhead is in while the transport plays (ui.bank
 * only: the cursor, the pattern and the saved project are untouched; DRUM's grid keeps its bank, its keys edit what the
 * grid shows). The bank is left alone ("held") from the moment the cursor moves or a knob edits the cursor's step, or
 * step entry has the keys: it goes back to the cursor's, so what is being edited stays on the screen. Following starts
 * again after FOLLOW_MS without that, or once the playhead has gone one pass round the pattern, whichever is first. The
 * transport stopped, another track, or the page left: nothing held any more */
#define FOLLOW_MS 3000u
static struct {
    uint32_t t;                                        /* fm1_ms of the last touch of the cursor's step */
    uint16_t prev, steps;                              /* the playhead at the last poll, the steps it has gone since */
    uint8_t hold;
} follow;
static void follow_hold(void)
{
    follow.hold = 1;
    follow.t = fm1_ms;
    follow.prev = TSEL->seq_idx;
    follow.steps = 0;
    ui.bank = (uint8_t)(ui.cursor / 16u);
}
static void follow_poll(void)                          /* ui_input, every pass */
{
    uint32_t len = (uint32_t)TSEL->p[P_SLEN], want, g = cur_page()->graph;
    if (!song.playing || ui.home || ui.menu || ui.confirm || drum_track(TSEL) ||
        !(step_page(g) || g == GR_CHANCE || g == GR_PLAY)) {
        follow.hold = 0;
        return;
    }
    if (len <= 16u)
        return;
    if (ui.entry_open)                                 /* step entry has the keys: stay at the cursor */
        follow_hold();
    if (follow.hold) {
        uint32_t idx = TSEL->seq_idx;
        follow.steps = (uint16_t)(follow.steps + (idx + len - follow.prev) % len);
        follow.prev = (uint16_t)idx;
        if (fm1_ms - follow.t < FOLLOW_MS && follow.steps < len)
            return;
        follow.hold = 0;
    }
    want = TSEL->seq_idx / 16u;
    if (want * 16u < len && want != ui.bank)
        ui.bank = (uint8_t)want;
}

/* SEQ cursor: wraps inside the pattern length, the bank follows, a step entry ends ((1.3) the bank stays there a while,
 * follow_hold) */
static void cursor_set(int32_t c)
{
    int32_t len = TSEL->p[P_SLEN] > 0 ? TSEL->p[P_SLEN] : 1;
    ui.cursor = (uint8_t)((c % len + len) % len);
    ui.bank = (uint8_t)(ui.cursor / 16u);
    ui.entry_open = 0;
    follow_hold();
}

static void cursor_fix(void)                           /* LEN got shorter: onto the last step */
{
    if (ui.cursor >= (uint32_t)TSEL->p[P_SLEN])
        cursor_set(TSEL->p[P_SLEN] - 1);
}

static void note_name(char *b, uint32_t n)
{
    str_cpy(b, N_NOTE[n % 12u], 4);
    fmt_int(b + str_len(b), (int32_t)(n / 12u) - 1);
}

/* (Skiff U3, N5) a family with no page on this track's engine: its button says so ("NO SCALE ON DRUM"), the page stays */
static void page_none(const char *title)
{
    char b[24];
    str_cpy(b, "NO ", sizeof b);
    str_cpy(b + 3, title, sizeof b - 3u);
    str_cpy(b + str_len(b), " ON ", sizeof b - str_len(b));
    ui_say(b, ENGINES[TSEL->eng_req % NENGINES]->name);
}
static void open_family(uint32_t fam)
{
    if (page_vis_first(fam) == NPAGES) {
        page_none(PAGES[page_first(fam)].title);
        return;
    }
    if (!ui.home && cur_page()->fam == fam) {          /* same button again: next page */
        uint32_t n, i = ui.page;
        for (n = 0; n < NPAGES; n++) {
            i = (i + 1u) % NPAGES;
            if (PAGES[i].fam == fam && page_visible(i)) break;
        }
        ui.page = (uint8_t)i;
    } else {                                           /* ((Skiff U3, N3) the page it was left on, SAVE's too: USER the
                                                        * first time, the sound save screen) */
        uint32_t last = ui.fam_last[fam], i;
        if (!last && fam == FAM_SAVE)
            for (last = 0; last < NPAGES && PAGES[last].graph != GR_USER; last++)
                ;
        i = last < NPAGES && PAGES[last].fam == fam && page_visible(last) ? last : page_vis_first(fam);
        ui.page = (uint8_t)i;
    }
    ui.fam_last[fam] = ui.page;
    ui.home = 0;
    page_entered();
}

/* GLO: MIXER, then (Skiff U3, N4) the buses DELAY REVERB CHORUS and GLOBAL, one family; taps step them round. From
 * another family ((U3, N3) as every family) the page it was left on: the mixer the first time. (ui.fam_last[FAM_GLO]
 * holds it, MIXER's FAM_TRK too) */
static void open_global(void)
{
    uint32_t i, last = ui.fam_last[FAM_GLO];
    if (!ui.home && (cur_page()->fam == FAM_TRK || cur_page()->fam == FAM_GLO)) {
        for (i = ui.page + 1u; i < NPAGES && PAGES[i].fam != FAM_GLO; i++) {}
        ui.page = (uint8_t)(cur_page()->fam == FAM_TRK ? page_first(FAM_GLO) : i < NPAGES ? i : page_first(FAM_TRK));
    } else {
        ui.page = (uint8_t)(last && last < NPAGES && (PAGES[last].fam == FAM_GLO || PAGES[last].fam == FAM_TRK) ? last :
                            page_first(FAM_TRK));
    }
    ui.fam_last[FAM_GLO] = ui.page;
    ui.home = 0;
    page_entered();
}

/* (Skiff phase 8, DESIGN.md rule 3) SELECT: the page before (dir < 0) or after this one in its family, in the order its
 * button steps them (GLO's MIXER, then GLOBAL and SYSTEM: one family), the ends where they are (no wrap); 1 = moved */
static uint32_t page_group(uint32_t i) { return PAGES[i].fam == FAM_TRK ? FAM_GLO : PAGES[i].fam; }
static int32_t page_order(uint32_t i) { return PAGES[i].fam == FAM_TRK ? -1 : (int32_t)i; }
static int page_step(int32_t dir)
{
    uint32_t g = page_group(ui.page), i, best = NPAGES;
    int32_t o = page_order(ui.page);
    if (ui.home)
        return 0;
    for (i = 0; i < NPAGES; i++) {
        int32_t oi = page_order(i);
        if (page_group(i) != g || !page_visible(i) || (dir > 0 ? oi <= o : oi >= o))
            continue;
        if (best == NPAGES || (dir > 0 ? oi < page_order(best) : oi > page_order(best)))
            best = i;
    }
    if (best == NPAGES)
        return 0;
    ui.page = (uint8_t)best;
    ui.fam_last[g] = (uint8_t)best;                     /* ((U3) GLO's group: MIXER's too) */
    page_entered();
    return 1;
}
/* the header's title (BLOCKS): the page's, (phase 8) DRUM-X's sheet with the sound ("SOUND SD") */
static const char *page_head(void)
{
    const page_t *pg = cur_page();
    const sheet_t *sh = sheet_of(pg->graph);
    if (sh)
        return sh->title();
    if (pg->scope == SC_ENGINE)                         /* ((Skiff U4, L5) EDIT: the engine's own name (OSC, KIT), the
                                                         * footer only where it is (n/m); the footer said it before) */
        return ENGINES[TSEL->eng_req % NENGINES]->page_title[pg->id[0] != P_E0];
    return pg->title;
}

static void go_home(void)
{
    ui.home = 1;
    ui.plk_src = 0;                              /* (parameter locks: HOME's four) */
    ui.entry_open = 0;
    ui.hot_t = 0;
    song.seq_mode = 0;
    ui.force = 1;
}

/* ------------------------------------------------------- track setup --- */
static int seq_is_empty(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (t->step[i].n || t->step[i].hit)
            return 0;
    return 1;
}

/* UNDO of a load. A sound load (a factory or user preset, an engine jump, TOOLS INIT, the
 * editor's PRESET / G_ENGSEL / UP_LOAD) changes the sound only; a pattern load (SAVE > PATTERNS) changes
 * the steps and the pattern parameters only. Each first copies the track as it was; SAVE held 0.7 s
 * swaps back what the loads changed, so steps recorded after a sound load, or a sound edited after a
 * pattern load, stay as they are. Skiff: UNDO_N copies, any tracks (undo_ring below): SAVE held goes one
 * back, then OCT- further, OCT+ forward (undo_step). Loads in a row on one track with nothing changed in
 * between (the PRESETS knob through the list, one pattern after the other, an editor audition) keep the
 * copy from before the first, so one undo goes back past the whole browse. Not snapshotted: power-on, projects.
 * 1.1.5: a lock or automation edit by hand (a step held and a knob turned, a step's locks cleared, SEQ > AUTO LIST)
 * takes the copy too (UNDO_MOT: SAVE held puts the track's motion back, the sound and the steps stay; motion_undo_take).
 * A sound load keeps the track's motion on the parameters every engine has (load_end, motion.c motion_sound_loaded);
 * a pattern load drops it all (the steps it was on are gone) */
enum { UNDO_SOUND = 1, UNDO_PAT = 2, UNDO_MOT = 4, UNDO_KIT = 8 };   /* (UNDO_KIT, Skiff phase 7: INIT on DRUM-X,
                                                                     * the project's kit: sound_init) */
typedef struct {
    uint8_t trk;                 /* track + 1, 0 = nothing to undo */
    uint8_t keep;                /* the track is as the last load left it (undo.after): a next load keeps the copy */
    uint8_t what;                /* UNDO_SOUND | UNDO_PAT: what the loads since the copy changed (undo_swap) */
    uint8_t eng, preset, user;
    uint8_t pid;                 /* (Skiff phase 5) the pattern the track was on (trk_pid): only there does it undo */
    uint8_t fm6_slot;            /* the track's FM6 patch and its SLOT (eng_fm6.c): an edited or a project's */
    uint8_t fm6[FP_SIZE + 1u];   /* patch is the track's own, not a factory one */
    dx_lane_t dx[DV_NLANE];      /* (Skiff phase 7, UNDO_KIT) the DRUM-X kit as it was */
    int16_t p[P_COUNT];
    step_t step[NSTEP];
    motion_store_t motion_backup; /* one track only, swaps with the shared event pool on undo */
    uint32_t after;              /* track_sig right after the last load */
    uint32_t t_ms;               /* time of the last load (the editor's SETs after it belong to it) */
    const char *name;            /* (Skiff) what it undoes: TAKE, CLEAR, PATTERN, SOUND, a tool's name (the undo box) */
} undo_t;
/* Skiff: UNDO_N copies in a ring (.ramx, after the RAM code: app.ld), the newest at undo_top; undo_back of the newest
 * are undone (each then holds the track as it was after it: its redo). A copy is one swap, so undo and redo are the
 * same act (undo_step). A new copy drops the undone ones (a new edit ends the redo); the oldest goes when all UNDO_N
 * are used. `undo` is the newest: loads in a row share it as before */
#define UNDO_N 8u
#define UNDO_WIN 180u                    /* the undo window, frames (~2.7 s): OCT- back, OCT+ redo (undo_step) */
static undo_t undo_ring[UNDO_N] __attribute__((section(".pool")));   /* ((PERNOTE, rev 5) .pool, not .ramx: with the
                                  * steps' note modes the ring no longer fit the 24 KiB RAMTEXT region (by 528 B); it is
                                  * the main loop's alone, as the other .pool buffers */
static uint8_t undo_n, undo_top, undo_back;
static const char *undo_label;   /* the next copy's name, set just before its load_begin (tl_undo_take,
                                  * motion_undo_take); 0: by what it changes */
#define undo (undo_ring[undo_top])
static void undo_clear(void) { undo_n = undo_back = 0; }   /* (a project load: the copies were the old project's) */
static void undo_push(uint32_t what)                       /* a new copy: the caller fills `undo` */
{
    undo_n = (uint8_t)(undo_n - undo_back);
    undo_top = (uint8_t)((undo_top + UNDO_N - undo_back + 1u) % UNDO_N);
    undo_back = 0;
    if (undo_n < UNDO_N)
        undo_n++;
    undo.name = undo_label ? undo_label : (what & UNDO_SOUND) ? "SOUND" : (what & UNDO_PAT) ? "PATTERN" : "AUTOMATION";
    undo_label = 0;
}
static uint8_t undo_depth;       /* loads nest (an engine jump loads its first preset): the outer one counts;
                                  * felucca_init / project_load raise it to take no copy at all */
/* (Skiff phase 5) each track's list (bank.c, DESIGN.md): NENT entries, byte 0 its pattern id + 1 (0: empty), byte 1
 * its repeats - 1; the entry each track is on; SONG on; the scenes (bank.c scene_*). A project keeps the same bytes
 * (SKF2). trk_pid: the pattern id + 1 the track's steps came from, where they are kept (0: none yet, given an id when
 * first kept). All 0 (power-on, a reset): nothing */
static struct {
    uint8_t ent[NTRK][NENT][2];
    uint8_t cur[NTRK];
    uint8_t song;
    uint8_t scene[PROJ_SCENES];
} lists __attribute__((section(".pool")));
static uint8_t trk_pid[NTRK] __attribute__((section(".pool")));   /* (.pool: off the ISR's merged globals) */
static uint32_t ent_pat(uint32_t k, uint32_t e)        /* entry e of track k: its pattern id, PAT_NONE: empty */
{
    uint32_t v = lists.ent[k % NTRK][e % NENT][0];
    return v ? v - 1u : PAT_NONE;
}
static uint32_t trk_pat(uint32_t k) { return trk_pid[k % NTRK] ? trk_pid[k % NTRK] - 1u : PAT_NONE; }
/* (Skiff phase 6) the first of the four tracks a screen shows (HOME, SONG, PERFORM, MIXER, the GLO cards): T1..T4, or
 * with T5 or T6 selected the four ending at it */
static uint32_t trk_top(void) { return song.sel > 3u ? song.sel - 3u : 0u; }
/* .. its scroll bar (design/mock_p6.py vbar, hbar): 2 px, the track LINE, the four shown MID; up and down (x0, y0 ..
 * y1) or across (x0 .. x1, y0) */
static void trk_bar(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int vert = y1 - y0 > x1 - x0;
    int32_t n = vert ? y1 - y0 : x1 - x0, t = (int32_t)trk_top();
    int32_t a = (2 * n * t + NTRK) / (2 * NTRK), b = (2 * n * (t + 4) + NTRK) / (2 * NTRK);
    cv_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, T_LINE);
    if (vert)
        cv_rect(x0, y0 + a, x1 - x0 + 1, b - a + 1, T_MID);
    else
        cv_rect(x0 + a, y0, b - a + 1, y1 - y0 + 1, T_MID);
}
static void lists_clear(void)                          /* no lists, no patterns on the tracks */
{
    memset(&lists, 0, sizeof lists);
    memset(trk_pid, 0, sizeof trk_pid);
}
static uint8_t load_from;        /* a sound load (the outer one): the engine before it (load_end) */
static uint8_t load_mo;          /* .. the track's motion paused while the sound changes: bit 0 set, bit 1 it was ON */

static uint32_t fnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t steps_sig(const track_t *t) { return fnv(2166136261u, t->step, sizeof t->step); }
static uint32_t track_sig(const track_t *t)      /* the sound (an FM6 track's patch, a DRUM-X track's kit), the steps */
{
    uint8_t id[3] = {t->eng_req, t->preset, t->user};
    uint32_t h = fnv(fnv(steps_sig(t), t->p, sizeof t->p), id, 3), k = trk_index(t);
    h = fnv(h, fm6_patch[k], sizeof fm6_patch[k]); /* (a patch the editor sent between two loads) */
    if (t->eng_req % NENGINES == ENGI_DRUMX)     /* ((Skiff phase 9) a kit edited between two preset loads) */
        h = fnv(h, dx_kit, sizeof dx_kit);
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}
static uint32_t motion_sig(const track_t *t)     /* the track's motion only (the sound moves while it plays) */
{
    uint32_t h = 2166136261u, k = trk_index(t);
    for (uint32_t j = 0; j < motion.count; j++)
        if ((motion.event[j].place >> 6) == k) h = fnv(h, &motion.event[j], sizeof motion.event[j]);
    return h ^ ((motion.on >> k) & 1u);
}

/* load_begin, the copy taken: a pattern load drops the track's motion (its steps go); a sound load pauses it (PLAY
 * OFF while the sound changes under it) and load_end keeps what still means the same (motion_sound_loaded) */
static void load_motion_hold(track_t *t, uint32_t what)
{
    if (what & UNDO_PAT) {
        motion_reset(t);
        load_mo = 0;
        return;
    }
    load_from = t->eng_req;
    load_mo = (uint8_t)(1u | (motion_enabled(t) ? 2u : 0u));
    motion_set_enabled(t, 0);
}

/* (Skiff 1.3, from Felucca 1.2) MOMENTARY (ui_input.c mom_turn: LFO held, the knobs' changes go back when it is let go):
 * the values turned, where they are (a track's p[], song.g[]) and what they were. In the main loop's pool (no .bss) */
#define MOM_N 8u
static struct {
    uint8_t n;                                    /* values held, 0 = none */
    int16_t *vp[MOM_N];
    int16_t v[MOM_N];
} mom __attribute__((section(".pool")));
static void mom_restore(void)                     /* LFO let go: every value back */
{
    while (mom.n) {
        mom.n--;
        *mom.vp[mom.n] = mom.v[mom.n];
    }
}
static void mom_back(const track_t *t)            /* a sound load into t (0: a project): its values back first */
{
    uint32_t i, k = 0;
    for (i = 0; i < mom.n; i++)
        if (!t || (mom.vp[i] >= t->p && mom.vp[i] < t->p + P_COUNT) || (mom.vp[i] >= song.g && mom.vp[i] < song.g + G_COUNT)) {
            *mom.vp[i] = mom.v[i];                /* (a global: back too, before the load's undo copy) */
        } else {
            mom.vp[k] = mom.vp[i];
            mom.v[k++] = mom.v[i];
        }
    mom.n = (uint8_t)k;
}
static void load_begin(track_t *t, uint32_t what)
{
    uint32_t i = trk_index(t);
    if (what & UNDO_SOUND) {
        mom_back(t);                              /* ((1.3) MOMENTARY: the sound as it was, then the load) */
        drum_mute[i] = 0;                         /* ((1.3) the lanes' mutes go with the sound) */
    }
    if (undo_depth++)
        return;
    motion_restore(t);
    if (undo_n && !undo_back && undo.keep && undo.trk == i + 1u && !(undo.what & UNDO_MOT) &&
        track_sig(t) == undo.after) {
        if ((what & UNDO_KIT) && !(undo.what & UNDO_KIT))
            memcpy(undo.dx, dx_kit, sizeof undo.dx);   /* ((phase 7) the kit as it was, if not held already) */
        undo.what |= (uint8_t)what;               /* browsing on: the copy from before the first load stays */
        undo_label = 0;
        load_motion_hold(t, what);
        return;
    }
    undo_push(what);
    undo.trk = (uint8_t)(i + 1u);
    undo.what = (uint8_t)what;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    memcpy(undo.fm6, fm6_patch[i], FP_SIZE);
    undo.fm6_slot = fm6_slot[i];
    if (what & UNDO_KIT)
        memcpy(undo.dx, dx_kit, sizeof undo.dx);
    undo.pid = trk_pid[i];
    motion_snapshot_track(t, &undo.motion_backup);
    load_motion_hold(t, what);
}

static void load_end(track_t *t)
{
    if (--undo_depth)
        return;
    if (load_mo & 1u) {                           /* a sound load: the motion on the common parameters stays */
        (void)motion_sound_loaded(t, load_from, t->eng_req);
        if ((load_mo & 2u) && motion_count(t))
            motion_set_enabled(t, 1);
        load_mo = 0;
    }
    motion_rebase(t);
    undo.after = track_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}

#include "ui_take.c"                                  /* Skiff: SHIFT + SAVE, TAKES into the track (take_commit) */

/* the editor's SET right after a load on the selected track (an audition: G_ENGSEL, then the patch's
 * values): part of that load, the copy from before it stays */
static void load_extend(track_t *t)
{
    if (undo_n && !undo_back && undo.keep && undo.trk == trk_index(t) + 1u && !(undo.what & UNDO_MOT) &&
        fm1_ms - undo.t_ms < 1500u) {
        undo.after = track_sig(t);
        undo.t_ms = fm1_ms;
    }
}

/* 1.1.5: a lock / automation edit by hand on track t is about to happen: the undo copy of the track as it is (as
 * ui_tools.c tl_undo_take, UNDO_MOT: the swap puts back its motion only). A knob turned on (the same tag, not 0:
 * the same knob of the same gesture), on the same track, nothing else changed and less than 1.5 s apart, shares the
 * copy from before its first detent; tag 0 (an add, a delete, a clear, a kind) always takes its own.
 * motion_undo_done after each */
static uint32_t undo_mot_tag;
static void motion_undo_take(track_t *t, uint32_t tag)
{
    uint32_t i = trk_index(t);
    if (tag && tag == undo_mot_tag && undo_n && !undo_back && undo.keep && undo.trk == i + 1u &&
        undo.what == UNDO_MOT && motion_sig(t) == undo.after && fm1_ms - undo.t_ms < 1500u) {
        undo_label = 0;
        return;
    }
    undo_mot_tag = tag;
    undo_push(UNDO_MOT);
    undo.trk = (uint8_t)(i + 1u);
    undo.what = UNDO_MOT;
    undo.eng = t->eng_req;
    undo.preset = t->preset;
    undo.user = t->user;
    memcpy(undo.p, t->p, sizeof undo.p);
    memcpy(undo.step, t->step, sizeof undo.step);
    memcpy(undo.fm6, fm6_patch[i], FP_SIZE);
    undo.fm6_slot = fm6_slot[i];
    undo.pid = trk_pid[i];
    motion_snapshot_track(t, &undo.motion_backup);
    undo.keep = 0;
}
static void motion_undo_done(track_t *t)
{
    undo.after = motion_sig(t);
    undo.keep = 1;
    undo.t_ms = fm1_ms;
}

static int param_kept(uint32_t i);

/* a copy and its track change places: what its loads changed (c->what). The sound: engine, preset and its
 * parameters (not param_kept); the pattern: the steps and LEN DIV SWING GATE; the track's motion. The mix (LEVEL PAN
 * MUTE) stays: no load changes it. Swapped again, the track is back: one copy is its undo and its redo.
 * (Skiff phase 5) a copy of the steps or the motion undoes only on the pattern it was taken on (the track's steps are
 * kept as that pattern: another one's would go into this one). 0: nothing changed (AUTOMATION FULL, OTHER PATTERN) */
static int undo_swap(undo_t *c)
{
    track_t *t = &trk[(c->trk - 1u) % NTRK];
    uint32_t i;
    motion_store_t current_motion;
    if ((c->what & (UNDO_PAT | UNDO_MOT)) && c->pid != trk_pid[trk_index(t)]) {
        char b[24] = "UNDO NEEDS PATTERN 00";         /* ((Skiff U4) which one: its number) */
        b[19] = (char)('0' + (c->pid + 1u) / 10u % 10u);
        b[20] = (char)('0' + (c->pid + 1u) % 10u);
        ui_message(b);
        return 0;
    }
    motion_restore(t);
    motion_snapshot_track(t, &current_motion);
    if (motion_replace_track(t, &c->motion_backup) != 0) {
        ui_way("AUTOMATION FULL", WAY_MOTION);
        return 0;
    }
    c->motion_backup = current_motion;
    fm1_irq_off();                                /* the audio ISR must not see half a sound */
    if (c->what & UNDO_SOUND) {
        uint8_t e = t->eng_req, pr = t->preset, u = t->user;
        panic_req |= (uint8_t)(1u << trk_index(t));
        t->eng_req = c->eng;
        t->preset = c->preset;
        t->user = c->user;
        c->eng = e;
        c->preset = pr;
        c->user = u;
        for (i = 0; i < P_COUNT; i++)
            if (!param_kept(i)) {
                int16_t v = t->p[i];
                t->p[i] = c->p[i];
                c->p[i] = v;
            }
    }
    if (c->what & UNDO_PAT) {
        for (i = P_SLEN; i <= P_SGATE + 1u; i++) {    /* LEN DIV SWING GATE, (Skiff) and DIR */
            uint32_t k = i <= P_SGATE ? i : P_DIR;
            int16_t v = t->p[k];
            t->p[k] = c->p[k];
            c->p[k] = v;
        }
        for (i = 0; i < NSTEP; i++) {
            step_t s = t->step[i];
            t->step[i] = c->step[i];
            c->step[i] = s;
        }
    }
    if (c->what & UNDO_KIT) {                     /* (phase 7) INIT on DRUM-X: the project's kit as it was */
        dx_lane_t k[DV_NLANE];
        memcpy(k, dx_kit, sizeof k);
        memcpy(dx_kit, c->dx, sizeof k);
        memcpy(c->dx, k, sizeof k);
    }
    fm1_irq_on();
    if (c->what & UNDO_SOUND) {                   /* FM6: the track's patch as it was (edited, a project's, a
                                                   * converted DIGITAL sound), not a factory one */
        uint32_t tr = trk_index(t);
        uint8_t v[FP_SIZE + 1u], sl = fm6_slot[tr];
        memcpy(v, fm6_patch[tr], FP_SIZE);
        fm6_set_patch(tr, c->fm6);
        fm6_slot[tr] = c->fm6_slot;               /* (fm6_poll: the patch stays) */
        fm6_own_ok &= (uint8_t)~(1u << tr);
        memcpy(c->fm6, v, FP_SIZE);
        c->fm6_slot = sl;
    }
    c->keep = 0;                                  /* the next load copies the track as it is now */
    if (t == TSEL)
        sync_reload = 1;
    ui.force = 1;
    return 1;
}

/* Skiff: SAVE held, or OCT- in the undo window: back one copy; OCT+ in the window: forward one (its redo). Each opens
 * the window again (ui.undo_w: ui_input.c takes OCT- / OCT+ while it is open): the undo box says what and how far back
 * (BLOCKS, ui_take.c), else the header */
static void undo_step(int redo)
{
    undo_t *c;
    char b[24];
    if (redo ? !undo_back : undo_back >= undo_n) {
        ui_message(redo ? "NOTHING TO REDO" : "NOTHING TO UNDO");
        ui.undo_w = undo_n ? UNDO_WIN : 0u;       /* (the other way may still go) */
        return;
    }
    if (redo)
        undo_back--;
    c = &undo_ring[(undo_top + UNDO_N - undo_back) % UNDO_N];
    if (!undo_swap(c)) {
        if (redo)
            undo_back++;
        return;
    }
    if (!redo)
        undo_back++;
    if (take_box_fits()) {
        take_box_undo(redo, (uint32_t)c->trk - 1u, c->name);
    } else {                                      /* "UNDO T1 TAKE", then the keys */
        str_cpy(b, redo ? "REDO T1 " : "UNDO T1 ", sizeof b);
        b[6] = (char)('1' + (c->trk - 1u) % NTRK);
        ui_say(b, c->name);
        str_cpy(ui.msg2, "[OCT-] BACK  OCT+ REDO", sizeof ui.msg2);
    }
    ui.undo_w = UNDO_WIN;
}

static void track_defaults_steps(track_t *t)
{
    uint32_t i;
    motion_reset(t);
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
}


/* the track's settings, not the sound's: what a sound load (factory or user preset, an engine jump,
 * TOOLS INIT) leaves alone. The mix (LEVEL, PAN, MUTE: the TRACKS faders), the arpeggiator (ARP, ARP 2),
 * the scale and key map (SCL), the pattern parameters (LEN, DIV, SWING, GATE) and the SLICER insert,
 * which chops whatever the track plays in time with its sequencer */
static int param_kept(uint32_t i)
{
    return i == P_LEVEL || i == P_PAN || i == P_MUTE || (i >= P_AMODE && i <= P_SGATE) ||
           (i >= P_SLCR && i <= P_SLDEPTH) || i == P_CHRD || i == P_VOIC || i == P_DIR ||   /* ((Skiff) DIR: the pattern's; */
           (i >= P_DKSRC && i <= P_DKREL);           /* the ducking: the mix's) */
}

/* a retired preset kept as an alias, so stored preset numbers stay valid: SAMPLE 1, once TRANH, is PIANO
 * (tools/gen_samples.py SMP_SET_ORIG). It loads as the original; browsing skips it. -> the preset k stands for.
 * (SAMPLE 4, once PERC: apply_preset_to loads it as DRUM; (Skiff phase 9) SAMPLE's variations follow it, so it is an
 * alias among the presets, skipped as 1 is) */
static uint32_t preset_orig(const engine_t *e, uint32_t k)
{
    return e->presets == SMP_PRESET_TABLE && k < SMP_NSETS ? SMP_SET_ORIG[k] : k;
}

/* the presets of engine e that browsing shows before preset k (k = npresets: all of them) */
static uint32_t preset_rank(const engine_t *e, uint32_t k)
{
    uint32_t i, n = 0;
    if (e->presets != SMP_PRESET_TABLE)
        return k;
    for (i = 0; i < k; i++)
        n += preset_orig(e, i) == i;
    return n;
}

#define preset_shown(e) (ENGINES[e]->npresets - (ENGINES[e]->presets == SMP_PRESET_TABLE ? SMP_NALIAS : 0u))

/* (Skiff 1.3, from Felucca 1.5.1, its #179 by renebohne) MIDI Program Change: program n on track t, as the PRESETS list
 * numbers the engine's sounds from 0: its n-th preset, the aliases skipped (preset_orig), loaded as from there (one
 * undo for a run of them). A program past the last is ignored, not wrapped: a number is always the same sound */
static void apply_preset_to(track_t *t, uint32_t pi);
static volatile uint8_t *const mpc_io = ui.mpc;
static void midi_pc_load(track_t *t, uint32_t n)
{
    uint32_t e = t->eng_req % NENGINES, k;
    if (n >= preset_shown(e))
        return;
    for (k = 0; preset_orig(ENGINES[e], k) != k || n--; k++)   /* the n-th shown preset */
        ;
    apply_preset_to(t, k);
}
static void midi_pc_poll(void)                       /* main loop (ui_input) */
{
    uint32_t k, v;
    for (k = 0; k < NTRK; k++) {
        if (!ui.mpc[k])
            continue;
        fm1_irq_off();                               /* (the ISR may note another meanwhile) */
        v = ui.mpc[k];
        ui.mpc[k] = 0;
        fm1_irq_on();
        midi_pc_load(&trk[k], v & 127u);
        ui.force = 1;
    }
}
#include "midi_learn.c"                           /* (1.3) MIDI LEARN: MENU > MIDI, a knob, a CC */
#define preset_listed(e) (eng_allowed(song.sel, e) ? preset_shown(e) : 0u)   /* ((Skiff phase 6) the PRESETS list: what
                                                                         * the selected track may play) */

#if !FELUCCA_FM4
/* DIGITAL (engine 1, retired): t's sound = p, values as DIGITAL has them, converted to FM6 with a patch of its own
 * (fm4_convert.c). Every path that brings a DIGITAL sound into a track ends here: a track never keeps engine 1 */
static void fm4_apply(track_t *t, int16_t *p)
{
    uint8_t v[FP_SIZE + 1u];
    uint32_t pr = fm4_convert(p, v), tr = trk_index(t), f;
    fm6_set_patch(tr, v);
    f = motion_guard();                               /* the audio ISR sees the old sound or the new one */
    memcpy(t->p, p, sizeof t->p);
    t->eng_req = ENGI_FM6;
    t->preset = (uint8_t)pr;
    motion_unguard(f);
    fm6_adopt(tr);                                    /* SLOT OWN: the converted patch is the track's own */
}
static void fm4_track(track_t *t)                     /* t holds a DIGITAL sound (engine 1): convert it */
{
    int16_t p[P_COUNT];
    memcpy(p, t->p, sizeof p);
    fm4_apply(t, p);
}
/* DIGITAL preset k (a stored preset number of engine 1: the editor's PRESET, SET G_ENGSEL, an old project's
 * power-on sound): its sound as a preset load sets it, converted. The sound only (not the steps, not param_kept) */
static void fm4_load_preset(track_t *t, uint32_t k)
{
    int16_t p[P_COUNT];
    uint32_t i;
    load_begin(t, UNDO_SOUND);
    panic_req |= (uint8_t)(1u << trk_index(t));
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    memcpy(p, t->p, sizeof p);
    for (i = 0; i < P_E0; i++)
        if (!param_kept(i))
            p[i] = TP[i].def;
    fm4_preset_values(p, k);
    fm4_apply(t, p);
    load_end(t);
}
#endif

static int set_engine_of(track_t *t, uint32_t ei);
/* preset pi of the engine the track asked for: the sound only (not the steps, not param_kept) */
static void apply_preset_to(track_t *t, uint32_t pi)
{
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    uint32_t i, kit;
#if !FELUCCA_FM4
    if (t->eng_req % NENGINES == ENGI_DIGITAL) {
        fm4_load_preset(t, pi);
        return;
    }
#endif
    if (t->eng_req % NENGINES == ENGI_SAMPLE && pi == SMP_SET_PERC) {   /* SAMPLE preset 4 was PERC (retired, a stored */
        set_engine_of(t, ENGI_DRUM);                  /* number: the editor's PRESET, a favourite): DRUM's kit */
        return;                                       /* (core.h drum_from_perc) */
    }
    kit = t->eng_req % NENGINES == ENGI_DRUMX ? DRUMX_PKIT[pi % NELEM(DRUMX_PKIT)] : 0u;   /* ((phase 9) the kit + 1) */
    load_begin(t, UNDO_SOUND | (kit ? UNDO_KIT : 0u));
    panic_req |= (uint8_t)(1u << trk_index(t));       /* MONO/POLY may change: release what sounds */
    t->user = 0;
    if (t == TSEL)
        sync_reload = 1;
    if (!e->npresets) {
        load_end(t);
        return;
    }
    pi = preset_orig(e, pi % e->npresets);
    t->preset = (uint8_t)pi;
    for (i = 0; i < P_E0; i++)                        /* the rest of the sound to its defaults: a preset */
        if (!param_kept(i))                           /* sounds the same after any edit */
            t->p[i] = TP[i].def;
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = (int16_t)e->presets[pi].e[i];
    t->p[P_ATK] = e->presets[pi].env[0];
    t->p[P_DEC] = e->presets[pi].env[1];
    t->p[P_SUS] = e->presets[pi].env[2];
    t->p[P_REL] = e->presets[pi].env[3];
    t->p[P_ED_FLT] = e->presets[pi].fenv;
    t->p[P_VOICE] = e->presets[pi].mono ? V_LEGATO : V_POLY;   /* mono presets keep the legato feel */
    {   /* the sends */
        static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
        const preset_t *pr = &e->presets[pi];
        for (i = 0; i < 4u; i++)
            t->p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
        for (i = 0; i < NELEM(pr->more) && pr->more[i].id; i++)   /* ((Skiff) its LFO, GLIDE, ..: core.h) */
            t->p[pr->more[i].id] = pr->more[i].v;
    }
    if (kit)                                          /* ((Skiff phase 9) DRUM-X: the preset's kit, the project's: */
        memcpy(dx_kit, DX_KITS[kit - 1u], sizeof dx_kit);   /* one undo copy holds it, as INIT's: sound_init) */
    fm6_track_loaded(t);                              /* FM6: the preset's patch (its SLOT) */
    load_end(t);
}

/* (Skiff phase 6) an engine a light track may not play (engines.c eng_allowed), asked for: said, nothing changes */
static void eng_refused(uint32_t e) { ui_say(ENGINES[e % NENGINES]->name, ": T1-T4 ONLY"); }

/* the engine's defaults and its first preset. With the audio IRQ off: the ISR sees the old engine with
 * its values or the new one with its own (voice.c engine_block), never one with the other's. 0 done; 1 refused (an
 * engine the track may not play: said, nothing changed, no undo copy) */
static int set_engine_of(track_t *t, uint32_t ei)
{
    const engine_t *e = ENGINES[ei % NENGINES];
    uint32_t i;
    if (!eng_allowed(trk_index(t), ei)) {
        eng_refused(ei);
        return 1;
    }
#if !FELUCCA_FM4
    if (ei % NENGINES == ENGI_DIGITAL) {             /* DIGITAL (retired): its first preset, as FM6 */
        fm4_load_preset(t, 0);
        return 0;
    }
#endif
    load_begin(t, UNDO_SOUND);
    fm1_irq_off();
    t->eng_req = (uint8_t)(ei % NENGINES);
    for (i = 0; i < 8u; i++)
        t->p[P_E0 + i] = e->edit[i].def;
    apply_preset_to(t, 0);
    fm1_irq_on();
    load_end(t);
    return 0;
}

static void apply_preset(uint32_t pi) { apply_preset_to(TSEL, pi); }
static int set_engine(uint32_t ei) { return set_engine_of(TSEL, ei); }
/* INIT (TOOLS, the EDIT layer): the selected track's engine from its defaults and first preset, the steps kept;
 * (Skiff phase 7) on a DRUM-X track the project's kit back to the factory's too. One undo copy holds both (UNDO_KIT).
 * (FM6 LAB) on FM6 the bare sine to build a sound from (FM6_INIT: OP1 alone, ALG 1; SLOT OWN; the macros at 0) */
static void sound_init(void)
{
    track_t *t = TSEL;
    int kit = t->eng_req % NENGINES == ENGI_DRUMX;
    load_begin(t, UNDO_SOUND | (kit ? UNDO_KIT : 0u));   /* (set_engine's load nests in it) */
    set_engine(t->eng_req);
    if (kit)
        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    if (t->eng_req % NENGINES == ENGI_FM6) {
        uint8_t v[FP_SIZE + 1u];
        fm6_unpack(FM6_INIT, v);
        fm6_set_patch(trk_index(t), v);
        fm6_adopt(trk_index(t));
    }
    load_end(t);
    ui_message("SOUND INITIALIZED");
}

static void track_defaults(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_E0; i++)
        t->p[i] = TP[i].def;
    track_defaults_steps(t);
}

/* switch engine (its defaults + first preset) and say so; 1: refused (said) */
static int select_engine(uint32_t e)
{
    ui.force = 1;
    if (set_engine(e))
        return 1;
    ui_say("ENGINE ", ENGINES[TSEL->eng_req]->name);
    return 0;
}

/* the presets of every engine (in ENGINE_ORDER), then the used user presets, as one list (the PRESETS knob and the
 * PRESETS page browse it) */
static uint32_t preset_all_pos(uint32_t *total)          /* list index of the selected track's preset */
{
    uint32_t n = 0, cur = 0, e, r;
    for (r = 0; r < NENG_SHOWN; r++) {                  /* (engines.c ENGINE_ORDER) */
        const engine_t *en = ENGINES[e = eng_vis(r)];
        if (e == TSEL->eng_req)
            cur = n + preset_rank(en, preset_orig(en, TSEL->preset % (en->npresets ? en->npresets : 1u)));
        n += preset_listed(e);
    }
    if (user_of(TSEL) < UP_SLOTS)
        cur = n + up_rank(user_of(TSEL));
    *total = n + up_count();
    return cur;
}

/* list index n (< total) -> engine, *k its preset; NENGINES = user preset, *k its slot */
static uint32_t preset_all_at(uint32_t n, uint32_t *k)
{
    uint32_t e = 0, i, r;
    for (r = 0; r < NENG_SHOWN && n >= preset_listed(eng_vis(r)); r++)
        n -= preset_listed(eng_vis(r));
    if (r == NENG_SHOWN) {
        *k = up_nth(n);
        return NENGINES;
    }
    e = eng_vis(r);
    for (i = 0; preset_orig(ENGINES[e], i) != i || n--; i++)    /* the n-th shown preset */
        ;
    *k = i;
    return e;
}

/* (Skiff phase 9) the LIST (PRESETS KNOB 4): 0 ALL, 1 FAV (favorites.filter), 2.. a type (ui_ptag + 1) */
static uint32_t list_mode(void) { return ui_ptag ? ui_ptag + 1u : favorites.filter; }
static const char *const LIST_NAME[PT_N + 1] = {"ALL", "FAV", "BASS", "LEAD", "PAD", "KEYS", "PLUCK", "DRUM", "FX"};
static int preset_in_list(uint32_t e, uint32_t k)     /* a LIST but ALL: the sound is in it (user presets: FAV only) */
{
    if (ui_ptag)
        return e < NENGINES && ENGINES[e]->presets[k].tag == ui_ptag;
    return favorite_has(e, k);
}

static uint32_t preset_pos(uint32_t *total)
{
    uint32_t all, current = preset_all_pos(&all), n = 0, pos = 0xFFFFFFFFu;
    if (!list_mode()) { *total = all; return current; }
    for (uint32_t i = 0; i < all; i++) {
        uint32_t k, e = preset_all_at(i, &k);
        if (!preset_in_list(e, k)) continue;
        if (i == current) pos = n;
        n++;
    }
    *total = n;
    return pos == 0xFFFFFFFFu ? n : pos; /* current sound need not be in the list */
}
static uint32_t preset_at(uint32_t n, uint32_t *k)
{
    uint32_t all;
    if (!list_mode()) return preset_all_at(n, k);
    preset_all_pos(&all);
    for (uint32_t i = 0; i < all; i++) {
        uint32_t e = preset_all_at(i, k);
        if (preset_in_list(e, *k) && !n--) return e;
    }
    *k = UP_SLOTS;
    return NENGINES;
}
static int preset_favorite(void)
{
    uint32_t k = user_of(TSEL);
    return favorite_has(k < UP_SLOTS ? NENGINES : TSEL->eng_req,
                                        k < UP_SLOTS ? k : TSEL->preset);
}
/* (Felucca #197) a settings change made by a knob (PRESETS > LIST, the FAV mark on PRESETS and in the EDIT layer) is
 * written later, as MENU's are when it closes: a write erases a flash sector with the interrupts off (~40 ms), which
 * stops the LED scan (a flash of the whole panel) and the audio. Written once the view it was made in is left
 * (another page, HOME, a layer shown or let go, the MENU) or after SET_LATER_MS without panel input (scrn.idle),
 * never on every detent; settings_poll still waits for the transport */
#define SET_LATER_MS 10000u
static struct { uint8_t on, page, home, layer; } set_later __attribute__((section(".pool")));
static void settings_later(void)
{
    set_later.on = 1;
    set_later.page = ui.page;
    set_later.home = ui.home;
    set_later.layer = ui.layer;
}
static void settings_later_poll(void)                  /* ui_input, every pass */
{
    if (set_later.on && (set_later.page != ui.page || set_later.home != ui.home || set_later.layer != ui.layer ||
                         ui.menu || fm1_ms - scrn.idle >= SET_LATER_MS)) {
        set_later.on = 0;
        settings_save();
    }
}
static void preset_mark(int on)
{
    uint32_t k = user_of(TSEL);
    if (favorite_set(k < UP_SLOTS ? NENGINES : TSEL->eng_req, k < UP_SLOTS ? k : TSEL->preset, on)) {
        ui.force = 1;
        settings_later();
    }
}

static void preset_go(uint32_t n)                    /* load list index n into the selected track (the sound only) */
{
    uint32_t k, e = preset_at(n, &k);
    if (e == NENGINES) {
        up_load(k);
    } else if (e == TSEL->eng_req || !select_engine(e)) {   /* (refused: not its preset either) */
        apply_preset(k);
    }
    ui.force = 1;
}

/* the EDIT layer's KNOB 2 (ui_layer.c): the selected track's engine only: its factory presets, then the used user presets
 * made with it (slot order). List index of the current sound; *total the length */
static uint32_t eng_list_pos(uint32_t *total)
{
    const engine_t *en = ENGINES[TSEL->eng_req % NENGINES];
    uint32_t e = TSEL->eng_req % NENGINES, np = preset_shown(e), u = user_of(TSEL), k, n = 0;
    uint32_t cur = np ? preset_rank(en, preset_orig(en, TSEL->preset % en->npresets)) : 0u;   /* (#124: the aliases
                                        * skipped, as on PRESETS: SAMPLE 1 loads as 0, so counting it stuck KNOB 2) */
    for (k = 0; k < UP_SLOTS; k++)
        if (up_used(k) && up_engine(k) == e) {
            if (k == u)
                cur = np + n;
            n++;
        }
    *total = np + n;
    return cur;
}

static void eng_list_step(int32_t direction)         /* the next / previous sound of the engine (wraps) */
{
    uint32_t total, cur = eng_list_pos(&total), e = TSEL->eng_req % NENGINES, np = preset_shown(e), k, n;
    if (total < 2u)
        return;
    n = (cur + (direction > 0 ? 1u : total - 1u)) % total;
    if (n < np) {
        for (k = 0; preset_orig(ENGINES[e], k) != k || n--; k++)   /* the n-th shown preset */
            ;
        apply_preset(k);
    } else {
        n -= np;
        for (k = 0; k < UP_SLOTS; k++)
            if (up_used(k) && up_engine(k) == e && !n--) {
                up_load(k);
                break;
            }
    }
    ui.force = 1;
}

static void preset_step(int32_t direction)
{
    uint32_t total, cur = preset_pos(&total);
    if (!total) { ui_message(ui_ptag ? "NO SOUNDS" : "NO FAVORITES"); return; }
    preset_go(cur >= total ? (direction > 0 ? 0 : total - 1) :
              (cur + (direction > 0 ? 1u : total - 1u)) % total);
}

/* Seven display rows. A LIST of favorites or of a type uses a bounded window, not a repeating carousel.
 * Return total for an empty row; a current sound outside the list shows the start. */
static uint32_t preset_visible(uint32_t cur, uint32_t total, uint32_t row)
{
    uint32_t first, last;
    if (!total || row >= 7u) return total;
    if (!list_mode())
        return (cur + total * 4u + row - 3u) % total;
    first = cur < total && cur > 3u ? cur - 3u : 0u;
    last = total > 7u ? total - 7u : 0u;
    if (first > last) first = last;
    return first + row < total ? first + row : total;
}

/* HOME: what KNOB k edits: the engine's four main parameters */
static const param_desc_t *home_param(uint32_t k, int16_t **vp)
{
    uint32_t id = ENGINES[TSEL->eng_req % NENGINES]->knob[k & 3u];
    *vp = &TSEL->p[id];
    return track_desc(TSEL, id);
}

/* select track i (KNOB 1 on TRACKS, the editor): its sound, pages and pattern from now on */
static void track_select(uint32_t i)
{
    if (i >= NTRK || i == song.sel)
        return;
    song.sel = (uint8_t)i;
    ui.entry_open = 0;
    ui.hot_t = 0;
    ui.cursor = 0;
    ui.bank = 0;
    follow.hold = 0;                                   /* ((1.3) the roll follows the new track's playhead) */
    sync_reload = 1;
    ui.force = 1;
}

#include "ui_slice.c"                             /* EDIT > SLICES: SLICE's slices by hand (an action page too) */
#include "ui_events.c"                            /* SEQ > AUTO LIST: the locks and events as a list (an action page) */

/* ---------------------------------------------------- action pages --- */
/* Pages whose purpose is an action (SAVE > USER, PATTERNS, PROJECT, EDIT > SLICES; SEQ > AUTOMATION, SONG): the knobs pick,
 * OCT+ does it, OCT- cancels the picked action or goes HOME (ui_input.c). There OCT- / OCT+ do not
 * shift the octave */
static int go_id(uint32_t id) { return id == G_LOAD || id == G_SAVE; }

/* (Skiff phase 5) SEQ > SONG: pattern id as the page writes it, "01" .. "64" (b: 3 bytes) */
static void pat_num(char *b, uint32_t id)
{
    b[0] = (char)('0' + (id + 1u) / 10u % 10u);
    b[1] = (char)('0' + (id + 1u) % 10u);
    b[2] = 0;
}
/* .. KNOB 2's pick for the entry picked, while it is that track's and entry's (0: none) */
static uint32_t song_pick(void)
{
    return sg.pick && sg.pick_at == 1u + (song.sel % NTRK) * 16u + sg.entry % NENT ? sg.pick : 0u;
}
/* .. what OCT+ does there: the entry played ("PLAY"), the pick put into it ("SET 07"), a copy of its pattern made for it
 * ("COPY 06 TO 17") */
static const char *song_act_name(void)
{
    static char b[16] __attribute__((section(".pool")));   /* (.pool: off the ISR's merged globals) */
    uint32_t p = song_pick(), id = ent_pat(song.sel, sg.entry), n = pool_alloc();
    if (!p)
        return "PLAY";
    if (p == SPICK_CLEAR)
        return "CLEAR";
    if (p == SPICK_COPY) {
        str_cpy(b, "COPY ", sizeof b);
        pat_num(b + 5, id);
        str_cpy(b + 7, n < NPAT ? " TO " : "", sizeof b - 7u);
        if (n < NPAT)
            pat_num(b + 11, n);
        return b;
    }
    str_cpy(b, "SET ", sizeof b);
    pat_num(b + 4, p - 1u);
    return b;
}

static uint32_t act_cols(void)                   /* the columns that are actions, a bit each; 0 = not such a page */
{
    const page_t *pg = cur_page();
    uint32_t c, m = 0;
    if (ui.home)
        return 0;
    if (pg->graph == GR_EVENTS)                  /* AUTOMATION: no knob is an action, OCT+ adds / toggles (ev_oct); */
        return ui.ev_top ? 8u : 16u;             /* ((Skiff U3, R5) its top row: CLEAR on KNOB 4) */
    if (pg->graph == GR_SONG) return 16u;       /* (Skiff phase 5) SONG: so too; OCT+ plays or sets (song_oct) */
    if (pg->graph == GR_USER)
        return 14u;                              /* LOAD ERASE (or NAME, ui.act2) SAVE */
    if (pg->graph == GR_SLOTS)
        return 14u;                              /* ((Skiff U2) NAME, was EDIT; (1.2.1) DELETE or NAME) LOAD (or NEW) SAVE */
    if (pg->graph == GR_BANK)
        return 8u;                               /* ((Skiff phase 5) the pool view: CLEAR, always picked: act_col) */
    if (pg->graph == GR_SLICES)
        return slice_page_ok() ? 12u : 0u;       /* SPLIT JOIN (a SLICE track only) */
    if (pg->scope == SC_GLOBAL)
        for (c = 0; c < 4u; c++)
            if (go_id(pg->id[c]))
                m |= 1u << c;
    return m;
}

/* the action OCT+ does: its column + 1, 0 = none picked yet */
static uint32_t act_col(void)
{
    if (!ui.home && ((cur_page()->graph == GR_EVENTS && !ui.ev_top) || cur_page()->graph == GR_SONG)) return 5u;
    if (!ui.home && cur_page()->graph == GR_BANK) return 4u;
    return ui.act;
}

static const char *act_name(uint32_t c)          /* column c's action (the footer hint) */
{
    static const char *const UP_GO[3] = {"LOAD", "ERASE", "SAVE"};
    uint32_t id = cur_page()->id[c & 3u];
    if (cur_page()->graph == GR_EVENTS) return ui.ev_top ? "CLEAR" : ev_act_name();
    if (cur_page()->graph == GR_SONG) return song_act_name();
    if (cur_page()->graph == GR_USER)
        return c == 2u && ui.act2 ? "NAME" : UP_GO[(c + 2u) % 3u];
    if (cur_page()->graph == GR_SLOTS && c == 1u)
        return ui.act == 2u && ui.act2 ? "NAME" : "DELETE";   /* ((1.2.1, #6) DELETE, one step further NAME) */
    if (cur_page()->graph == GR_SLOTS && id == G_LOAD && !project_used((uint32_t)song.g[G_SLOT] - 1u))
        return "NEW";                            /* ((1.2.1, #6) an empty slot) */
    if (cur_page()->graph == GR_BANK)
        return "CLEAR";
    if (cur_page()->graph == GR_SLICES)
        return c == 3u ? "JOIN" : "SPLIT";
    return id == G_LOAD ? "LOAD" : "SAVE";
}

/* the picked action would do something now (OCT+ breathes): another pattern, a used slot, stopped for
 * a flash write, steps to clear */
static int act_ready(void)
{
    uint32_t c = act_col(), s = song.sel, id;
    if (!c--)
        return 0;
    if (cur_page()->graph == GR_EVENTS) return ui.ev_top ? motion_count(TSEL) != 0u : ev_act_ready();
    if (cur_page()->graph == GR_SONG)                   /* (Skiff phase 5) a pick, else an entry to play */
        return song_pick() == SPICK_COPY ? pool_alloc() < NPAT : song_pick() == SPICK_CLEAR ? bank_used(s, sg.entry) :
               song_pick() || bank_used(s, sg.entry);
    if (cur_page()->graph == GR_USER)
        return c == 3u ? !song.playing : up_used(ui.uslot) && (c == 1u || !song.playing);
    if (cur_page()->graph == GR_SLOTS && c == 1u)       /* ((Skiff U2) NAME; (1.2.1) DELETE) */
        return project_used((uint32_t)song.g[G_SLOT] - 1u) && !transport_busy();
    if (cur_page()->graph == GR_BANK)                   /* (Skiff phase 5) a pattern nothing uses, stopped */
        return pool_used(ui.bslot % NPAT) && !pool_refs(ui.bslot % NPAT) && !transport_busy();
#if FELUCCA_SLICE
    if (cur_page()->graph == GR_SLICES)
        return slice_act_ready(c);
#endif
    id = cur_page()->id[c & 3u];
    if (id == G_LOAD)                                   /* ((1.2.1, #6) an empty slot: NEW, always) */
        return 1;
    return !song.playing;                               /* (G_SAVE) */
}
