// .nex memory layout: code directly after the ULA screen+attributes
// (0x4000-0x5AFF), stack at the top of memory.  The core is ~37 KB of
// code+rodata, too big for the classic 0x8000 window; sample banking
// (8K MMU pages) arrives with the sound milestone.
#pragma output CRT_ORG_CODE = 0x6000
// stack below the IM2 jump stub at 0xFDFD (table 0xFE00-0xFF00)
#pragma output REGISTER_SP  = 0xFDFC
#pragma output CRT_ENABLE_CLOSE = 0
#pragma output CLIB_MALLOC_HEAP_SIZE = 0
#pragma output CLIB_STDIO_HEAP_SIZE  = 0
#pragma output CLIB_EXIT_STACK_SIZE  = 0
