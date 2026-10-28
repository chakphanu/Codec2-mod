/*
  c2m_public_only - the clean-header mode of codec2_mod.h.

  Built with -std=c99 -Wpedantic and CODEC2_MOD_PUBLIC_ONLY: the build
  fails if the header pulls in the internal configuration macros, and the
  program only uses the public API with opaque, heap-allocated state.

  usage: c2m_public_only input.raw

  (p1) codec2_decode() equals codec2_decode_params() with the caller's own
       codec2_params_state + codec2_params_to_model(NULL) + codec2_synthesise()
  (p2) codec2_pack_params() of the decoded second half (v0 from the first
       bit) reproduces the transmitted frame
*/
#define CODEC2_MOD_PUBLIC_ONLY
#include "codec2_mod.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* none of the internal configuration macros may leak in this mode */
#if defined(T) || defined(DEC) || defined(COEFF) || defined(N_S) || defined(TWO_PI) || defined(M_PI) || \
    defined(LPC_ORD) || defined(P_MAX) || defined(P_MIN) || defined(WO_BITS) || defined(E_BITS) ||       \
    defined(LSPD_SCALAR_INDEXES) || defined(MAX_AMP) || defined(FFT_ENC) || defined(FFT_DEC) ||          \
    defined(N_SAMP) || defined(W0_MIN) || defined(W0_MAX) || defined(BYTES_PER_FRAME) ||                 \
    defined(SAMPLES_PER_FRAME) || defined(POW10F) || defined(NW) || defined(TW) || defined(CNLP) ||     \
    defined(CODEC2_MOD_INTERNAL_H) || defined(KISS_FFT_H)
#error "codec2_mod.h leaked internal macros in CODEC2_MOD_PUBLIC_ONLY mode"
#endif

/* aligned allocation without C11 aligned_alloc */
static void *alloc_aligned(size_t size, void **base)
{
    size_t align = codec2_state_alignment();
    uintptr_t p;

    *base = malloc(size + align - 1);
    if (!*base)
        return NULL;
    p = ((uintptr_t)*base + align - 1) / align * align;
    return (void *)p;
}

int main(int argc, char **argv)
{
    void *enc_base, *dec1_base, *dec2_base;
    codec2_encoder_t *enc;
    codec2_decoder_t *dec1, *dec2;
    codec2_params_state st;
    codec2_half_params p[2];
    codec2_model_t m[2];
    codec2_complex_t Aw[2][CODEC2_AW_BINS];
    int16_t speech[CODEC2_SAMPLES_PER_FRAME];
    int16_t out1[CODEC2_SAMPLES_PER_FRAME], out2[CODEC2_SAMPLES_PER_FRAME];
    uint8_t bits[CODEC2_BYTES_PER_FRAME], repacked[CODEC2_BYTES_PER_FRAME];
    unsigned long frames = 0, pcm_diffs = 0, pack_diffs = 0;
    FILE *f;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s input.raw\n", argv[0]);
        return 2;
    }
    f = fopen(argv[1], "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    enc = alloc_aligned(codec2_encoder_size(), &enc_base);
    dec1 = alloc_aligned(codec2_decoder_size(), &dec1_base);
    dec2 = alloc_aligned(codec2_decoder_size(), &dec2_base);
    if (!enc || !dec1 || !dec2)
        return 2;

    codec2_encoder_init(enc);
    codec2_decoder_init(dec1);
    codec2_decoder_init(dec2);
    codec2_params_init(&st);

    while (fread(speech, sizeof(int16_t), CODEC2_SAMPLES_PER_FRAME, f) == CODEC2_SAMPLES_PER_FRAME)
    {
        codec2_encode(enc, bits, speech);

        codec2_decode(dec1, out1, bits);

        codec2_decode_params(&st, bits, p);
        codec2_params_to_model(dec2, p, NULL, m, Aw);
        codec2_synthesise(dec2, m, (const codec2_complex_t(*)[CODEC2_AW_BINS])Aw, out2);
        pcm_diffs += memcmp(out1, out2, sizeof(out1)) != 0;

        codec2_pack_params(repacked, bits[0] >> 7, &p[1]);
        pack_diffs += memcmp(repacked, bits, sizeof(bits)) != 0;

        frames++;
    }
    fclose(f);

    printf("%s (CODEC2_MOD_PUBLIC_ONLY), encoder %lu B, decoder %lu B, alignment %lu\n", codec2_mod_version(),
           (unsigned long)codec2_encoder_size(), (unsigned long)codec2_decoder_size(),
           (unsigned long)codec2_state_alignment());
    printf("%s (p1) codec2_decode == decode_params + params_to_model + synthesise: %lu/%lu frames identical\n",
           pcm_diffs ? "FAIL" : "PASS", frames - pcm_diffs, frames);
    printf("%s (p2) pack_params(decode_params(bits)) == bits: %lu/%lu frames identical\n",
           pack_diffs ? "FAIL" : "PASS", frames - pack_diffs, frames);

    free(enc_base);
    free(dec1_base);
    free(dec2_base);
    return (pcm_diffs || pack_diffs) ? 1 : 0;
}
