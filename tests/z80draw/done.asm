; done.asm - test-completion landmark for z88dk-ticks.
; run.sh resolves _test_done from the map file and passes it to
; ticks -end: the emulator exits (and writes the -output RAM dump) the
; moment PC reaches this address.  The jr-to-self is a safety net if
; -end were mis-set; the -w watchdog would then terminate the run.
SECTION code_user
PUBLIC _test_done
_test_done:
    jr   _test_done
