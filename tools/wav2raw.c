/* wav2raw: strip RIFF header from 8-bit mono PCM WAV and decimate by an
 * integer factor with boxcar averaging.  Build: gcc -O2 -o wav2raw wav2raw.c
 * Usage: wav2raw in.wav out.raw [factor]      (factor default 1)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RIFF_HDR_MIN 44
#define CHUNK_ID_LEN 4

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s in.wav out.raw [factor]\n", argv[0]);
        return 2;
    }
    int factor = (argc > 3) ? atoi(argv[3]) : 1;
    if (factor < 1 || factor > 8) {
        fprintf(stderr, "factor must be 1..8\n");
        return 2;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror(argv[1]); return 1; }
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);
    if (size < RIFF_HDR_MIN) {
        fprintf(stderr, "%s: too small for a WAV\n", argv[1]);
        fclose(in);
        return 1;
    }
    uint8_t *buf = malloc((size_t)size);
    if (!buf || fread(buf, 1, (size_t)size, in) != (size_t)size) {
        fprintf(stderr, "%s: read failed\n", argv[1]);
        free(buf);
        fclose(in);
        return 1;
    }
    fclose(in);

    if (memcmp(buf, "RIFF", CHUNK_ID_LEN) != 0 ||
        memcmp(buf + 8, "WAVE", CHUNK_ID_LEN) != 0) {
        fprintf(stderr, "%s: not a RIFF WAVE\n", argv[1]);
        free(buf);
        return 1;
    }

    /* walk chunks: require PCM(1), 1 channel, 8-bit; find data */
    long pos = 12;
    const uint8_t *data = NULL;
    uint32_t dlen = 0;
    int fmt_ok = 0;
    while (pos + 8 <= size) {
        const uint8_t *ck = buf + pos;
        uint32_t cklen = rd32(ck + 4);
        if (memcmp(ck, "fmt ", CHUNK_ID_LEN) == 0 && cklen >= 16) {
            uint16_t audiofmt = (uint16_t)(ck[8] | (ck[9] << 8));
            uint16_t channels = (uint16_t)(ck[10] | (ck[11] << 8));
            uint16_t bits = (uint16_t)(ck[22] | (ck[23] << 8));
            if (audiofmt != 1 || channels != 1 || bits != 8) {
                fprintf(stderr, "%s: need 8-bit mono PCM (fmt=%u ch=%u "
                        "bits=%u)\n", argv[1], audiofmt, channels, bits);
                free(buf);
                return 1;
            }
            fmt_ok = 1;
        } else if (memcmp(ck, "data", CHUNK_ID_LEN) == 0) {
            data = ck + 8;
            dlen = cklen;
            if (data + dlen > buf + size) {
                dlen = (uint32_t)(buf + size - data);
            }
        }
        pos += 8 + cklen + (cklen & 1);
    }
    if (!fmt_ok || !data) {
        fprintf(stderr, "%s: missing fmt/data chunk\n", argv[1]);
        free(buf);
        return 1;
    }

    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); free(buf); return 1; }
    uint32_t written = 0;
    for (uint32_t i = 0; i + (uint32_t)factor <= dlen; i += (uint32_t)factor) {
        unsigned acc = 0;
        for (int k = 0; k < factor; ++k) {
            acc += data[i + (uint32_t)k];
        }
        fputc((int)(acc / (unsigned)factor), out);
        ++written;
    }
    fclose(out);
    free(buf);
    printf("%s: %u samples -> %u @ /%d\n", argv[2], dlen, written, factor);
    return 0;
}
