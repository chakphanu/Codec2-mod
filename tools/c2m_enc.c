/*
  c2m_enc - Codec2 3200 encoder: raw 8 kHz s16le mono -> 8-byte frames.

  usage: c2m_enc in.raw out.bit      ("-" = stdin / stdout)

  Uses only the public API: codec2_mod.h in CODEC2_MOD_PUBLIC_ONLY mode
  (opaque state allocation).
*/
#define CODEC2_MOD_PUBLIC_ONLY
#include "codec2_mod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static FILE *open_or_std(const char *path, const char *mode, FILE *std)
{
    return strcmp(path, "-") ? fopen(path, mode) : std;
}

static void *alloc_state(size_t size)
{
    size_t align = codec2_state_alignment();
    size = (size + align - 1) / align * align;
    return aligned_alloc(align, size);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s in.raw out.bit   (\"-\" = stdin/stdout)\n", argv[0]);
        return 2;
    }

    FILE *fin = open_or_std(argv[1], "rb", stdin);
    FILE *fout = open_or_std(argv[2], "wb", stdout);
    if (!fin || !fout)
    {
        fprintf(stderr, "cannot open input or output\n");
        return 2;
    }

    codec2_encoder_t *enc = alloc_state(codec2_encoder_size());
    if (!enc)
        return 2;
    codec2_encoder_init(enc);

    int16_t speech[CODEC2_SAMPLES_PER_FRAME];
    uint8_t bits[CODEC2_BYTES_PER_FRAME];

    while (fread(speech, sizeof(int16_t), CODEC2_SAMPLES_PER_FRAME, fin) == CODEC2_SAMPLES_PER_FRAME)
    {
        codec2_encode(enc, bits, speech);
        if (fwrite(bits, 1, sizeof(bits), fout) != sizeof(bits))
            return 1;
    }

    free(enc);
    if (fin != stdin)
        fclose(fin);
    if (fout != stdout)
        fclose(fout);
    return 0;
}
