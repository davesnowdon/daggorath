/* Sound effect IDs - indices match the original port's sound/ numbering.
 * The platform layer maps an id to its PCM asset; the core only speaks ids.
 */
#ifndef DOD_SOUND_IDS_H
#define DOD_SOUND_IDS_H

enum {
    SND_SQUEAK = 0x00,  /* spider */
    SND_RATTLE = 0x01,
    SND_GROWL  = 0x02,
    SND_BEOOP  = 0x03,
    SND_KLANK  = 0x04,
    SND_GRAWL  = 0x05,
    SND_PSSST  = 0x06,
    SND_KKLANK = 0x07,
    SND_PSSHT  = 0x08,
    SND_SNARL  = 0x09,
    SND_BDLBDL1 = 0x0A,
    SND_BDLBDL2 = 0x0B,
    SND_GLUGLG = 0x0C,
    SND_PHASER = 0x0D,
    SND_WHOOP  = 0x0E,
    SND_CLANG  = 0x0F,
    SND_WHOOSH = 0x10,
    SND_CHUCK  = 0x11,
    SND_KLINK  = 0x12,
    SND_CLANK  = 0x13,
    SND_THUD   = 0x14,
    SND_BANG   = 0x15,
    SND_KABOOM = 0x16,
    SND_HEART1 = 0x17,  /* lub */
    SND_HEART2 = 0x18,  /* dub */
    SND_BUZZ   = 0x19,
    SND_COUNT  = 0x1A
};

#endif /* DOD_SOUND_IDS_H */
