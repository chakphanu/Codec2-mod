/*
  c2m_dec - Codec2 3200 decoder: 8-byte frames -> raw 8 kHz s16le mono.

  usage: c2m_dec [options] in.bit out.raw      ("-" = stdin / stdout)

  options (decoder-side only, the bitstream is unchanged):
    --no-pf           LPC post filter off
    --no-bass-boost   bass boost off
    --beta X          post filter beta (default 0.2)
    --gamma X         post filter gamma (default 0.5)
    --no-lpc-corr     first harmonic LPC correction off
    --seed N          phase generator seed (default 1)

  Without options the output equals codec2_decode(). Uses only the public
  API: codec2_mod.h in CODEC2_MOD_PUBLIC_ONLY mode (opaque state
  allocation).
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

static int usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [--no-pf] [--no-bass-boost] [--beta X] [--gamma X]\n"
            "          [--no-lpc-corr] [--seed N] in.bit out.raw   (\"-\" = stdin/stdout)\n",
            argv0);
    return 2;
}

int main(int argc, char **argv)
{
    codec2_model_opts opts;
    unsigned long seed = 1;
    int a = 1;

    codec2_model_opts_default(&opts);

    for (; a < argc && !strncmp(argv[a], "--", 2); a++)
    {
        if (!strcmp(argv[a], "--no-pf"))
            opts.post_filter = 0;
        else if (!strcmp(argv[a], "--no-bass-boost"))
            opts.bass_boost = 0;
        else if (!strcmp(argv[a], "--no-lpc-corr"))
            opts.lpc_correction = 0;
        else if (!strcmp(argv[a], "--beta") && a + 1 < argc)
            opts.beta = strtof(argv[++a], NULL);
        else if (!strcmp(argv[a], "--gamma") && a + 1 < argc)
            opts.gamma = strtof(argv[++a], NULL);
        else if (!strcmp(argv[a], "--seed") && a + 1 < argc)
            seed = strtoul(argv[++a], NULL, 0);
        else
            return usage(argv[0]);
    }
    if (argc - a != 2)
        return usage(argv[0]);

    FILE *fin = open_or_std(argv[a], "rb", stdin);
    FILE *fout = open_or_std(argv[a + 1], "wb", stdout);
    if (!fin || !fout)
    {
        fprintf(stderr, "cannot open input or output\n");
        return 2;
    }

    codec2_decoder_t *dec = alloc_state(codec2_decoder_size());
    if (!dec)
        return 2;
    codec2_decoder_init(dec);
    codec2_decoder_reseed(dec, (uint32_t)seed);

    codec2_params_state params;
    codec2_half_params p[2];
    codec2_model_t m[2];
    codec2_complex_t Aw[2][CODEC2_AW_BINS];
    uint8_t bits[CODEC2_BYTES_PER_FRAME];
    int16_t speech[CODEC2_SAMPLES_PER_FRAME];

    /* the decoder state is opaque here, so the parameter history is kept
       in a codec2_params_state of our own (same initial values) */
    codec2_params_init(&params);

    while (fread(bits, 1, sizeof(bits), fin) == sizeof(bits))
    {
        codec2_decode_params(&params, bits, p);
        codec2_params_to_model(dec, p, &opts, m, Aw);
        codec2_synthesise(dec, m, (const codec2_complex_t(*)[CODEC2_AW_BINS])Aw, speech);
        if (fwrite(speech, sizeof(int16_t), CODEC2_SAMPLES_PER_FRAME, fout) != CODEC2_SAMPLES_PER_FRAME)
            return 1;
    }

    free(dec);
    if (fin != stdin)
        fclose(fin);
    if (fout != stdout)
        fclose(fout);
    return 0;
}
