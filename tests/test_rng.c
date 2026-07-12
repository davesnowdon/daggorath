/* test_rng.c - core RNG must be byte-identical to the C++ port's RNG
 * (goldens generated from the port's own class by tools/make-goldens.sh),
 * which is itself the byte-exact RANDOM.ASM algorithm. */
#include "test_util.h"
#include "../core/rng.h"

static const dodBYTE LEVTAB[7] = {0x73, 0xC7, 0x5D, 0x97, 0xF3, 0x13, 0x87};
#define STREAM_LEN 4096

int main(void)
{
    for (int level = 0; level < 5; ++level) {
        char path[256];
        long glen = 0;
        snprintf(path, sizeof path, "golden/rng-level%d.bin", level);
        unsigned char *gold = t_read_file(path, &glen);
        T_CHECK(gold != NULL && glen == STREAM_LEN,
                "golden %s missing/short", path);
        if (!gold) {
            continue;
        }
        rng_set_seed(LEVTAB[level], LEVTAB[level + 1], LEVTAB[level + 2]);
        int bad = -1;
        for (int i = 0; i < STREAM_LEN; ++i) {
            if (rng_RANDOM() != gold[i]) {
                bad = i;
                break;
            }
        }
        T_CHECK(bad < 0, "level %d diverges from port at byte %d",
                level, bad);
        free(gold);
    }
    return t_report("test_rng");
}
