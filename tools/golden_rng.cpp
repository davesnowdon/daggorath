// golden_rng.cpp - emit golden RNG streams using the C++ PORT'S OWN RNG
// class (inline in its dod.h), so tests compare against the shipped code,
// not a re-implementation.  Build via tools/make-goldens.sh.
#include <cstdio>
#include "dod.h"   // the port's header: defines class RNG inline

static const dodBYTE LEVTAB[7] = {0x73, 0xC7, 0x5D, 0x97, 0xF3, 0x13, 0x87};
static const int STREAM_LEN = 4096;

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <outdir>\n", argv[0]);
        return 2;
    }
    RNG r;
    for (int level = 0; level < 5; ++level) {
        char path[512];
        std::snprintf(path, sizeof path, "%s/rng-level%d.bin", argv[1], level);
        std::FILE *f = std::fopen(path, "wb");
        if (!f) { std::perror(path); return 1; }
        r.setSEED(LEVTAB[level], LEVTAB[level + 1], LEVTAB[level + 2]);
        for (int i = 0; i < STREAM_LEN; ++i) {
            std::fputc(r.RANDOM(), f);
        }
        std::fclose(f);
        std::printf("wrote %s\n", path);
    }
    return 0;
}
