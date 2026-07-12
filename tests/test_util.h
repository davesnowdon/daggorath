/* test_util.h - minimal assert harness shared by all host tests. */
#ifndef DOD_TEST_UTIL_H
#define DOD_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int t_failures = 0;
static int t_checks = 0;

#define T_CHECK(cond, ...) do {                                   \
        ++t_checks;                                               \
        if (!(cond)) {                                            \
            ++t_failures;                                         \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);  \
            fprintf(stderr, __VA_ARGS__);                         \
            fprintf(stderr, "\n");                                \
        }                                                         \
    } while (0)

static inline int t_report(const char *name)
{
    if (t_failures == 0) {
        printf("PASS %s (%d checks)\n", name, t_checks);
        return 0;
    }
    printf("FAIL %s (%d/%d checks failed)\n", name, t_failures, t_checks);
    return 1;
}

static inline unsigned char *t_read_file(const char *path, long *len_out)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long len;
    if (!f) {
        fprintf(stderr, "cannot open golden file %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    *len_out = len;
    return buf;
}

#endif /* DOD_TEST_UTIL_H */
