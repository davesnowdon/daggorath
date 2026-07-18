/* desktop/main.c - SDL2 reference backend for the Daggorath core.
 *
 * Implements every plat_* entry point over SDL2 plus verification
 * machinery: --record/--replay (jiffy-stamped keys), --turbo (virtual
 * clock for deterministic max-speed replays), --screenshot N,
 * --pattern (render the verified shape/font tables without game logic).
 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/platform.h"
#include "../core/draw_ref.h"
#include "../core/tables_vec.h"
#include "../core/tables_font.h"
#include "../core/sound_ids.h"
#include "../core/game.h"
#include "../core/viewer.h"
#include "../core/player.h"
#include "../core/object.h"
#include "../core/creature.h"
#include "../core/dungeon.h"
#include "../core/parser.h"
#include "../core/sched.h"
#include "../core/rng.h"

#define WIN_SCALE 4
#define SFX_RATE 22050u
#define GLYPH_ROWS 7

/* ---- framebuffer ------------------------------------------------------ */
static uint8_t fb[FB_HEIGHT * FB_WIDTH];   /* 1 byte per pixel, 0/1 */

/* ---- options / shell state ------------------------------------------- */
static struct {
    int headless;
    int turbo;              /* virtual clock: 1 jiffy per yield */
    int white;              /* white phosphor instead of green */
    const char *record_path;
    const char *replay_path;
    const char *shot_path;
    unsigned shot_jiffy;
    unsigned exit_jiffy;    /* stop after N jiffies (0 = never) */
    int pattern;
    const char *dump_path;  /* state dump at every command prompt */
    const char *save_path;  /* ZSAVE/ZLOAD slot file */
} opt;

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_AudioDeviceID audio;
static int quit_requested;

/* virtual clock (turbo/replay determinism) */
static uint32_t sim_jiffies;
static uint64_t start_ms;

/* sound assets */
static uint8_t *sfx_data[SND_COUNT];
static uint32_t sfx_len[SND_COUNT];
static uint32_t snd_end_jiffy;   /* virtual-clock "playing until" */

/* record / replay */
static FILE *rec_file;
static FILE *rep_file;
static long rep_next_jiffy = -1;
static int rep_next_key = -1;

/* key queue from SDL events */
#define KEYQ 64
static int16_t keyq[KEYQ];
static int keyq_h, keyq_t;

static uint32_t now_jiffies32(void)
{
    if (opt.turbo) {
        return sim_jiffies;
    }
    return (uint32_t)(((SDL_GetTicks64() - start_ms) * 3u) / 50u);
}

/* ---- platform implementation ------------------------------------------ */
jiffy_t plat_jiffies(void)
{
    return (jiffy_t)now_jiffies32();
}

void plat_clear(void)
{
    memset(fb, 0, sizeof fb);
}

void plat_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint8_t vctfad, uint8_t flags)
{
    draw_line_ref(fb, x0, y0, x1, y1, vctfad,
                  (uint8_t)(flags & PLAT_LINE_INVERSE), 0, FB_HEIGHT);
}

void plat_blit_glyph(uint8_t col, uint8_t row, const uint8_t rows[GLYPH_ROWS])
{
    unsigned x0 = (unsigned)col * 8u, y0 = (unsigned)row * 8u, r, b;
    if (col >= 32u || row >= 24u) {
        return;
    }
    for (r = 0; r < GLYPH_ROWS; ++r) {
        for (b = 0; b < 8u; ++b) {
            fb[(y0 + r) * FB_WIDTH + x0 + b] =
                (uint8_t)((rows[r] >> (7u - b)) & 1u);
        }
    }
}

void plat_invert_region(uint8_t col, uint8_t row, uint8_t ncols)
{
    unsigned x0 = (unsigned)col * 8u, y0 = (unsigned)row * 8u, r, x;
    for (r = 0; r < GLYPH_ROWS; ++r) {
        for (x = 0; x < (unsigned)ncols * 8u && x0 + x < FB_WIDTH; ++x) {
            fb[(y0 + r) * FB_WIDTH + x0 + x] ^= 1u;
        }
    }
}

static void write_pgm(const char *path)
{
    FILE *f = fopen(path, "wb");
    size_t i;
    if (!f) {
        perror(path);
        return;
    }
    fprintf(f, "P5\n%u %u\n255\n", FB_WIDTH, FB_HEIGHT);
    for (i = 0; i < sizeof fb; ++i) {
        fputc(fb[i] ? 255 : 0, f);
    }
    fclose(f);
    fprintf(stderr, "screenshot -> %s\n", path);
}

static void dump_grid(const char *name, const uint8_t *g, int rows)
{
    int r, c;
    fprintf(stderr, "--- %s ---\n", name);
    for (r = 0; r < rows; ++r) {
        for (c = 0; c < 32; ++c) {
            fprintf(stderr, "%02X ", g[r * 32 + c]);
        }
        fputc('\n', stderr);
    }
}

static int shot_done;
static void shot_check(void)
{
    if (opt.shot_path && !shot_done && now_jiffies32() >= opt.shot_jiffy) {
        write_pgm(opt.shot_path);
        if (getenv("DOD_DUMP_TEXT")) {
            dump_grid("examArea", viewer.examArea, 19);
            dump_grid("textArea", viewer.textArea, 4);
            dump_grid("statArea", viewer.statArea, 1);
        }
        shot_done = 1;
        if (opt.headless) {
            quit_requested = 1;
        }
    }
}

void plat_present(void)
{
    shot_check();
    if (opt.headless) {
        return;
    }
    {
        uint32_t *px;
        int pitch;
        uint32_t on = opt.white ? 0xFFFFFFFFu : 0xFF33FF66u;
        size_t i;
        SDL_LockTexture(tex, NULL, (void **)&px, &pitch);
        for (i = 0; i < sizeof fb; ++i) {
            px[i] = fb[i] ? on : 0xFF000000u;
        }
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
    }
}

void plat_sound_play(uint8_t sound_id, uint8_t volume)
{
    if (sound_id >= SND_COUNT || !sfx_data[sound_id]) {
        return;
    }
    snd_end_jiffy = now_jiffies32()
        + (uint32_t)(((uint64_t)sfx_len[sound_id] * JIFFY_HZ) / SFX_RATE) + 1u;
    if (opt.headless) {
        return;
    }
    SDL_ClearQueuedAudio(audio);
    {   /* scale u8 samples about the 0x80 midline by volume/255 */
        uint32_t n = sfx_len[sound_id];
        uint8_t *tmp = malloc(n);
        uint32_t i;
        if (!tmp) {
            return;
        }
        for (i = 0; i < n; ++i) {
            int s = (int)sfx_data[sound_id][i] - 128;
            tmp[i] = (uint8_t)(128 + (s * volume) / 255);
        }
        SDL_QueueAudio(audio, tmp, n);
        free(tmp);
    }
}

void plat_sound_stop(void)
{
    snd_end_jiffy = 0;
    if (!opt.headless) {
        SDL_ClearQueuedAudio(audio);
    }
}

uint8_t plat_sound_playing(void)
{
    if (opt.headless || opt.turbo) {
        return (uint8_t)(now_jiffies32() < snd_end_jiffy);
    }
    return (uint8_t)(SDL_GetQueuedAudioSize(audio) > 0);
}

static void keyq_push(int16_t k)
{
    int nt = (keyq_t + 1) % KEYQ;
    if (nt != keyq_h) {
        keyq[keyq_t] = k;
        keyq_t = nt;
    }
}

static void pump_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT ||
            (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) {
            quit_requested = 1;
        } else if (e.type == SDL_KEYDOWN) {
            SDL_Keycode s = e.key.keysym.sym;
            int16_t k = -1;
            if (s >= SDLK_a && s <= SDLK_z) {
                k = (int16_t)('A' + (s - SDLK_a));
            } else if (s == SDLK_RETURN) {
                k = '\r';
            } else if (s == SDLK_BACKSPACE) {
                k = '\b';
            } else if (s == SDLK_SPACE) {
                k = ' ';
            }
            if (k >= 0) {
                if (rec_file) {
                    fprintf(rec_file, "%u %d\n", now_jiffies32(), k);
                }
                keyq_push(k);
            }
        }
    }
}

int16_t plat_poll_key(void)
{
    if (rep_file) {
        if (rep_next_jiffy < 0) {
            long j;
            int k;
            if (fscanf(rep_file, "%ld %d", &j, &k) == 2) {
                rep_next_jiffy = j;
                rep_next_key = k;
            } else {
                rep_next_jiffy = -2;   /* exhausted */
            }
        }
        if (rep_next_jiffy >= 0 &&
            (long)now_jiffies32() >= rep_next_jiffy) {
            int16_t k = (int16_t)rep_next_key;
            rep_next_jiffy = -1;
            return k;
        }
        return -1;
    }
    if (keyq_h != keyq_t) {
        int16_t k = keyq[keyq_h];
        keyq_h = (keyq_h + 1) % KEYQ;
        return k;
    }
    return -1;
}

void plat_yield(void)
{
    if (opt.turbo) {
        ++sim_jiffies;
    }
    shot_check();
    if (!opt.headless) {
        pump_events();
        if (!opt.turbo) {
            SDL_Delay(1);
        }
    }
    if (opt.exit_jiffy && now_jiffies32() >= opt.exit_jiffy) {
        quit_requested = 1;
    }
    if (quit_requested) {
        /* game_run never returns; the shell owns termination */
        plat_shutdown();
        exit(0);
    }
}

/* Save-file envelope: 8-byte header ["D" "S" ver flags len16 ck16] in
 * front of the raw blob, validated on load - a truncated, corrupted or
 * different-build file is rejected (PLAT_ERR_IO -> CMDERR) instead of
 * restored as garbage state.  Same format on all backends; the payload
 * remains the compiler-ABI struct dump, so saves stay per-machine. */
#define SAVE_FMT_VER 1u

static uint16_t save_fletcher16(const uint8_t *p, uint16_t n)
{
    uint16_t s1 = 0, s2 = 0, i;
    for (i = 0; i < n; ++i) {
        s1 = (uint16_t)((s1 + p[i]) % 255u);
        s2 = (uint16_t)((s2 + s1) % 255u);
    }
    return (uint16_t)((s2 << 8) | s1);
}

uint8_t plat_save_state(const void *buf, uint16_t len)
{
    FILE *f;
    size_t n;
    uint16_t ck;
    uint8_t hdr[8];
    if (!opt.save_path) {
        return PLAT_ERR_UNSUPPORTED;
    }
    ck = save_fletcher16((const uint8_t *)buf, len);
    hdr[0] = 'D';
    hdr[1] = 'S';
    hdr[2] = SAVE_FMT_VER;
    hdr[3] = 0;
    hdr[4] = (uint8_t)(len & 0xFFu);
    hdr[5] = (uint8_t)(len >> 8);
    hdr[6] = (uint8_t)(ck & 0xFFu);
    hdr[7] = (uint8_t)(ck >> 8);
    f = fopen(opt.save_path, "wb");
    if (!f) {
        return PLAT_ERR_IO;
    }
    n = fwrite(hdr, 1, 8, f);
    n += fwrite(buf, 1, len, f);
    fclose(f);
    return (n == (size_t)len + 8) ? PLAT_OK : PLAT_ERR_IO;
}

uint8_t plat_load_state(void *buf, uint16_t len)
{
    FILE *f;
    size_t n;
    uint16_t ck;
    uint8_t hdr[8];
    if (!opt.save_path) {
        return PLAT_ERR_UNSUPPORTED;
    }
    f = fopen(opt.save_path, "rb");
    if (!f) {
        return PLAT_ERR_IO;
    }
    n = fread(hdr, 1, 8, f);
    n += fread(buf, 1, len, f);
    fclose(f);
    if (n != (size_t)len + 8) {
        return PLAT_ERR_IO;
    }
    ck = save_fletcher16((const uint8_t *)buf, len);
    if (hdr[0] != 'D' || hdr[1] != 'S' || hdr[2] != SAVE_FMT_VER ||
        hdr[4] != (uint8_t)(len & 0xFFu) ||
        hdr[5] != (uint8_t)(len >> 8) ||
        hdr[6] != (uint8_t)(ck & 0xFFu) ||
        hdr[7] != (uint8_t)(ck >> 8)) {
        return PLAT_ERR_IO;
    }
    return PLAT_OK;
}

/* ---- command-boundary state dumps (--dump-state) ----------------------
 * Fired by core_prompt_hook at every viewer_PROMPT.  One text block per
 * prompt: every gameplay-relevant field, plus the status/prompt text
 * decoded to ASCII and a maze CRC.  Under --turbo --replay the whole
 * file is bit-reproducible, so stored goldens freeze the core. */
static FILE *dump_file;
static unsigned dump_seq;

static uint32_t crc32_buf(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    int b;
    for (i = 0; i < n; ++i) {
        crc ^= p[i];
        for (b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return crc;
}

/* display code -> ASCII (dump readability only; not gameplay) */
static char code_ascii(uint8_t c)
{
    if (c == 0x00) return ' ';
    if (c >= 1 && c <= 26) return (char)('A' + c - 1);
    switch (c) {
    case 0x1B: return '!';
    case 0x1C: return '_';
    case 0x1D: return '?';
    case 0x1E: return '.';
    case 0x1F: return '/';   /* I_CR */
    case 0x20: case 0x21: case 0x22: case 0x23: return '#';  /* hearts */
    case 0x24: return '<';   /* I_BS */
    }
    return '~';
}

static void dump_text_row(const char *tag, const uint8_t *g)
{
    int i;
    fprintf(dump_file, "%s \"", tag);
    for (i = 0; i < TEXT_COLS; ++i) {
        fputc(code_ascii(g[i]), dump_file);
    }
    fprintf(dump_file, "\"\n");
}

static void dump_state(void)
{
    int i;

    fprintf(dump_file, "=== %u t %u\n", dump_seq++, (unsigned)sched.curTime);
    fprintf(dump_file, "GAME lvl %u aut %u won %u demoptr %u\n",
            game.LEVEL, game.AUTFLG, game.hasWon, game.DEMOPTR);
    fprintf(dump_file, "RNG %02X%02X%02X\n",
            rng.SEED[0], rng.SEED[1], rng.SEED[2]);
    fprintf(dump_file,
            "PLR r %u c %u d %u pow %u dam %u mgo %u mgd %u pho %u phd %u"
            " heartr %u heartf %u faint %u\n",
            player.PROW, player.PCOL, player.PDIR,
            PPOW, PDAM, PMGO, PMGD, PPHO, PPHD,
            player.HEARTR, player.HEARTF, player.FAINT);
    fprintf(dump_file,
            "HND l %d r %d bag %d torch %d wt %u rlite %u mlite %u"
            " vlite %u,%u,%u\n",
            player.PLHAND, player.PRHAND, player.BAGPTR, player.PTORCH,
            player.POBJWT, player.PRLITE, player.PMLITE,
            viewer.RLIGHT, viewer.MLIGHT, viewer.OLIGHT);
    fprintf(dump_file, "SCHED n %d z %u\n", sched.TCBPTR, sched.ZFLAG);
    for (i = 0; i < sched.TCBPTR && i < TCB_COUNT; ++i) {
        const Task *t = &sched.TCBLND[i];
        if (t->type == DOD_NONE) {
            continue;
        }
        fprintf(dump_file, "T%d typ %d dat %d frq %u due %d\n", i,
                t->type, t->data, (unsigned)t->frequency,
                (int)(int16_t)(t->next_time - sched.curTime));
    }
    for (i = 0; i < object.OCBPTR && i < 72; ++i) {
        const OCB *o = &object.OCBLND[i];
        fprintf(dump_file,
                "OBJ %d id %u typ %u ptr %d own %u rc %u,%u lvl %u"
                " x %u,%u,%u\n",
                i, o->obj_id, o->obj_type, o->P_OCPTR, o->P_OCOWN,
                o->P_OCROW, o->P_OCCOL, o->P_OCLVL,
                o->P_OCXX0, o->P_OCXX1, o->P_OCXX2);
    }
    for (i = 0; i < 32; ++i) {
        const CCB *c = &creature.CCBLND[i];
        if (c->P_CCUSE == 0) {
            continue;
        }
        fprintf(dump_file,
                "CRE %d id %u pow %u dam %u rc %u,%u dir %u obj %d\n",
                i, c->creature_id, c->P_CCPOW, c->P_CCDAM,
                c->P_CCROW, c->P_CCCOL, c->P_CCDIR, c->P_CCOBJ);
    }
    fprintf(dump_file, "MAZ %08X\n",
            crc32_buf(dungeon.MAZLND, 1024u));
    dump_text_row("STAT", viewer.statArea);
    for (i = 0; i < PROMPT_ROWS; ++i) {
        char tag[8];
        snprintf(tag, sizeof tag, "TXT%d", i);
        dump_text_row(tag, viewer.textArea + i * TEXT_COLS);
    }
    fflush(dump_file);
}

static void load_sfx(const char *dir)
{
    static const char *names[SND_COUNT] = {
        "00_squeak", "01_rattle", "02_growl", "03_beoop", "04_klank",
        "05_grawl", "06_pssst", "07_kklank", "08_pssht", "09_snarl",
        "0A_bdlbdl", "0B_bdlbdl", "0C_gluglg", "0D_phaser", "0E_whoop",
        "0F_clang", "10_whoosh", "11_chuck", "12_klink", "13_clank",
        "14_thud", "15_bang", "16_kaboom", "17_heart", "18_heart",
        "19_buzz"
    };
    int i;
    for (i = 0; i < SND_COUNT; ++i) {
        char path[512];
        FILE *f;
        long n;
        snprintf(path, sizeof path, "%s/%s.raw", dir, names[i]);
        f = fopen(path, "rb");
        if (!f) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fseek(f, 0, SEEK_SET);
        sfx_data[i] = malloc((size_t)n);
        if (sfx_data[i] && fread(sfx_data[i], 1, (size_t)n, f) == (size_t)n) {
            sfx_len[i] = (uint32_t)n;
        }
        fclose(f);
    }
}

void plat_init(void)
{
    if (!opt.headless) {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
        win = SDL_CreateWindow("Dungeons of Daggorath (core port)",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               FB_WIDTH * WIN_SCALE, FB_HEIGHT * WIN_SCALE, 0);
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_PRESENTVSYNC);
        tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                FB_WIDTH, FB_HEIGHT);
        {
            SDL_AudioSpec want = {0};
            want.freq = SFX_RATE;
            want.format = AUDIO_U8;
            want.channels = 1;
            want.samples = 512;
            audio = SDL_OpenAudioDevice(NULL, 0, &want, NULL, 0);
            SDL_PauseAudioDevice(audio, 0);
        }
    }
    start_ms = opt.headless ? 0 : SDL_GetTicks64();
    load_sfx("../assets/raw-22050");
}

void plat_shutdown(void)
{
    int i;
    for (i = 0; i < SND_COUNT; ++i) {
        free(sfx_data[i]);
    }
    if (rec_file) {
        fclose(rec_file);
    }
    if (rep_file) {
        fclose(rep_file);
    }
    if (!opt.headless) {
        SDL_Quit();
    }
}

/* ---- test-pattern mode: render verified tables without game logic ----- */
static void pattern_shape(const uint8_t *vla, int16_t ox, int16_t oy,
                          uint8_t vctfad)
{
    uint8_t nlists = vla[0];
    unsigned i = 1, l, p;
    for (l = 0; l < nlists; ++l) {
        uint8_t npts = vla[i++];
        for (p = 0; p + 1 < npts; ++p) {
            plat_draw_line((int16_t)(vla[i + 2 * p] + ox),
                           (int16_t)(vla[i + 2 * p + 1] + oy),
                           (int16_t)(vla[i + 2 * p + 2] + ox),
                           (int16_t)(vla[i + 2 * p + 3] + oy),
                           vctfad, 0);
        }
        i += 2u * npts;
    }
}

static void run_pattern(void)
{
    static const char msg[] = "DUNGEONS OF DAGGORATH";
    unsigned i;
    plat_clear();
    /* room walls solid, spider mid-fade, wizard heavy fade */
    pattern_shape(LWAL_VLA, 0, 0, 0);
    pattern_shape(RWAL_VLA, 0, 0, 0);
    pattern_shape(FWAL_VLA, 0, 0, 0);
    pattern_shape(SP_VLA, -60, -40, 0);
    pattern_shape(W1_VLA, 30, -40, 1);
    pattern_shape(TORC_VLA, 40, 30, 0);
    for (i = 0; msg[i]; ++i) {
        uint8_t code = (msg[i] == ' ') ? 0u : (uint8_t)(msg[i] - 'A' + 1);
        plat_blit_glyph((uint8_t)(5 + i), 21, FONT_NORMAL[code]);
    }
    plat_blit_glyph(15, 19, FONT_SPECIAL[2]);   /* large heart */
    plat_blit_glyph(16, 19, FONT_SPECIAL[3]);
    plat_present();
    while (!quit_requested) {
        plat_yield();
        if (opt.headless) {
            break;
        }
        plat_present();
        SDL_Delay(15);
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--pattern] [--headless] [--turbo] [--white]\n"
            "          [--record F] [--replay F] [--screenshot N F]\n"
            "          [--exit-after N] [--dump-state F] [--save-file F]\n",
            argv0);
}

int main(int argc, char **argv)
{
    int i;
    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--headless")) {
            opt.headless = 1;
            opt.turbo = 1;
        } else if (!strcmp(argv[i], "--turbo")) {
            opt.turbo = 1;
        } else if (!strcmp(argv[i], "--white")) {
            opt.white = 1;
        } else if (!strcmp(argv[i], "--pattern")) {
            opt.pattern = 1;
        } else if (!strcmp(argv[i], "--record") && i + 1 < argc) {
            opt.record_path = argv[++i];
        } else if (!strcmp(argv[i], "--replay") && i + 1 < argc) {
            opt.replay_path = argv[++i];
        } else if (!strcmp(argv[i], "--screenshot") && i + 2 < argc) {
            opt.shot_jiffy = (unsigned)strtoul(argv[++i], NULL, 10);
            opt.shot_path = argv[++i];
        } else if (!strcmp(argv[i], "--exit-after") && i + 1 < argc) {
            opt.exit_jiffy = (unsigned)strtoul(argv[++i], NULL, 10);
        } else if (!strcmp(argv[i], "--dump-state") && i + 1 < argc) {
            opt.dump_path = argv[++i];
        } else if (!strcmp(argv[i], "--save-file") && i + 1 < argc) {
            opt.save_path = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (opt.record_path) {
        rec_file = fopen(opt.record_path, "w");
    }
    if (opt.dump_path) {
        dump_file = fopen(opt.dump_path, "w");
        if (!dump_file) {
            perror(opt.dump_path);
            return 1;
        }
        core_prompt_hook = dump_state;
    }
    if (opt.replay_path) {
        rep_file = fopen(opt.replay_path, "r");
        if (!rep_file) {
            perror(opt.replay_path);
            return 1;
        }
    }

    plat_init();
    if (opt.pattern) {
        run_pattern();
    } else {
        game_run();   /* never returns except via quit -> exit below */
    }
    plat_shutdown();
    return 0;
}
