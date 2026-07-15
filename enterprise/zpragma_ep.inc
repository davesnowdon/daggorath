// Enterprise EXOS type-5 .com: loads and starts at 0100h.  The program
// (code + data + BSS + stack) lives in Z80 pages 0-2 (0100h-BFFFh, ~48K);
// page 3 (C000-FFFF) is repurposed as the video segment (framebuffer +
// LPT), so the stack MUST stay below C000h.
// NB: z88dk pragma lines take the WHOLE line as the directive - keep every
// comment on its own line, never trailing a #pragma (it corrupts zcc_opt.def).
// drop the MS-DOS re-run stub
#pragma output noprotectmsdos
// stack in page 2, clear of the VRAM page 3
#pragma output REGISTER_SP = 0xBF00
// the core is freestanding (only <stdint.h>/<string.h>); no heap needed
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE = 0
