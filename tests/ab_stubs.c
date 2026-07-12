/* ab_stubs.c - link-level stubs for the module A/B harness.
 *
 * The harness links the REAL core modules under comparison (dungeon,
 * parser, rng, object, creature, sched, player) and stubs everything
 * else: the game/viewer globals (game.c / viewer.c are not linked), the
 * viewer/sound/platform entry points those modules call, and the player
 * command handlers that live in player_cmds.c / player_move.c.  None of
 * the stubbed functions run during the A/B scenarios (core_pump is never
 * called); they only satisfy the linker.
 */
#include "dod_types.h"
#include "game.h"
#include "viewer.h"
#include "sound.h"
#include "platform.h"
#include "player.h"

/* globals normally defined by game.c / viewer.c */
game_state game;
viewer_state viewer;

const dodBYTE GAME_DEMO_CMDS[GAME_DEMO_LEN] = { 0 };

void game_COMINI(void) {}
void game_INIVU(void) {}
void game_Restart(void) {}
void game_WAIT(void) {}
void game_run(void) {}

/* viewer entry points (viewer*.c not linked) */
void viewer_Reset(void) {}
void viewer_draw_game(void) {}
int8_t viewer_LUKNEW(void) { return 0; }
void viewer_PUPDAT(void) {}
void viewer_STATUS(void) {}
void viewer_MAPPER(void) {}
void viewer_VIEWER(void) {}
void viewer_EXAMIN(void) {}
void viewer_PCRLF(void) {}
void viewer_PRTOBJ(int8_t X, uint8_t highlite) { (void)X; (void)highlite; }
void viewer_OUTCHR(dodBYTE c) { (void)c; }
void viewer_OUTSTR(const dodBYTE *s) { (void)s; }
void viewer_OUTSTI(const uint8_t *packed) { (void)packed; }
void viewer_PROMPT(void) {}
void viewer_CLRSCR(void) {}
void viewer_TXTXXX(dodBYTE c) { (void)c; }
void viewer_TXTSCR(void) {}
void viewer_clearArea(TXB *a) { (void)a; }
void viewer_drawArea(TXB *a) { (void)a; }
void viewer_drawTorchHighlite(void) {}
void viewer_clear_screen(void) {}
uint8_t viewer_ShowFade(dodBYTE fadeMode) { (void)fadeMode; return 0; }
void viewer_fade_start(dodBYTE fadeMode) { (void)fadeMode; }
uint8_t viewer_draw_fade(void) { return 1; }
void viewer_fade_abort(void) {}
void viewer_displayPrepare(void) {}
void viewer_displayCopyright(void) {}
void viewer_displayWelcomeMessage(void) {}
void viewer_displayDeath(void) {}
void viewer_displayWinner(void) {}
void viewer_displayEnough(void) {}
void viewer_SETFAD(void) {}
void viewer_SETSCL(void) {}
void viewer_setVidInv(uint8_t inv) { (void)inv; }

/* sound facade (sound.c not linked) */
void snd_play(uint8_t sound_id, uint8_t volume) { (void)sound_id; (void)volume; }
void snd_creature(uint8_t sound_id, uint8_t range) { (void)sound_id; (void)range; }
void snd_stop(void) {}
uint8_t snd_playing(void) { return 0; }
void snd_wait_done(void) {}

/* platform layer (no backend linked) */
int16_t plat_poll_key(void) { return -1; }
jiffy_t plat_jiffies(void) { return 0; }
void plat_yield(void) {}
uint8_t plat_save_state(const void *buf, uint16_t len)
{ (void)buf; (void)len; return PLAT_ERR_UNSUPPORTED; }
uint8_t plat_load_state(void *buf, uint16_t len)
{ (void)buf; (void)len; return PLAT_ERR_UNSUPPORTED; }

/* player command handlers (player_cmds.c / player_move.c not linked;
 * player_HUMAN never runs in the A/B scenarios) */
void player_PATTK(void) {}
void player_PCLIMB(void) {}
void player_PDROP(void) {}
void player_PEXAM(void) {}
void player_PGET(void) {}
void player_PINCAN(void) {}
void player_PLOOK(void) {}
void player_PMOVE(void) {}
void player_PPULL(void) {}
void player_PREVEA(void) {}
void player_PSTOW(void) {}
void player_PTURN(void) {}
void player_PUSE(void) {}
void player_PZLOAD(void) {}
void player_PZSAVE(void) {}
uint8_t player_PSTEP(dodBYTE dir) { (void)dir; return 0; }
void player_ShowTurn(dodBYTE A) { (void)A; }
