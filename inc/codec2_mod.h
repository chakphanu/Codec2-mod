#ifndef CODEC2_MOD_H
#define CODEC2_MOD_H

/*
  Public API of Codec2-mod (Codec2 3200 bps).

  Default: this header includes codec2_internal.h, exactly as on main, so
  the codec state structs are complete types and the internal
  configuration macros (LPC_ORD, TWO_PI, ...), model_t and complex_t are
  visible. Code that uses the exported internals (quantise.h, interp.h,
  lpc.h) keeps building unchanged.

  Clean mode: define CODEC2_MOD_PUBLIC_ONLY before including this header.
  Then codec2_internal.h is not included, no unprefixed macro is defined
  (only CODEC2_* and the include guards), the state types
  codec2_t / codec2_encoder_t / codec2_decoder_t are opaque, and the
  caller allocates codec2_encoder_size() / codec2_decoder_size() bytes
  aligned to codec2_state_alignment(). The header is C99 clean in this
  mode. The library itself is always built without it.

  The public types and sizes (codec2_model_t == model_t,
  codec2_complex_t == complex_t, codec2_half_params, codec2_params_state,
  CODEC2_* constants) come from codec2_mod_types.h, which the internal
  headers include as well, so every type has one definition.
*/

#include "codec2_mod_types.h"
#ifndef CODEC2_MOD_PUBLIC_ONLY
#include "codec2_internal.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct codec2_t codec2_t;
    typedef struct codec2_encoder_t codec2_encoder_t;
    typedef struct codec2_decoder_t codec2_decoder_t;

    void codec2_init(codec2_t *c2);
    void codec2_encoder_init(codec2_encoder_t *c2);
    void codec2_decoder_init(codec2_decoder_t *c2);

    void codec2_encode(
        codec2_encoder_t *c2,
        uint8_t *bits,
        const int16_t *speech);

    void codec2_decode(
        codec2_decoder_t *c2,
        int16_t *speech,
        const uint8_t *bits);

    /*
      Layered decoder. codec2_decode(c2, speech, bits) is exactly

          codec2_half_params p[2];
          codec2_model_t m[2];
          codec2_complex_t Aw[2][CODEC2_AW_BINS];

          codec2_decode_params(&c2->params, bits, p);
          codec2_params_to_model(c2, p, NULL, m, Aw);
          codec2_synthesise(c2, m, Aw, speech);

      and each layer may be used on its own, e.g. a caller that only needs
      the decoded parameters (a neural vocoder front end) runs
      codec2_decode_params() with its own codec2_params_state.
    */

    /* Parameter layer (shared with Codec2-mod-neural). */

    /* initial history, the same values codec2_decoder_init() uses */
    void codec2_params_init(codec2_params_state *st);

    /* Unpack and dequantise one frame and interpolate the first half:
       p[0] = first 10 ms (Wo / voicing / energy / LSPs interpolated from
       the previous frame), p[1] = second 10 ms (as transmitted). Updates
       st. This is the parameter half of codec2_decode(). */
    void codec2_decode_params(
        codec2_params_state *st,
        const uint8_t bits[CODEC2_BYTES_PER_FRAME],
        codec2_half_params p[2]);

    /*
      Model layer. Decoder-side options with the meaning of the pf,
      bass_boost, beta and gamma arguments of upstream Codec2's aks_to_M2()
      and its apply_lpc_correction() step. codec2_model_opts_default() sets
      the values codec2_decode() uses: post filter on, bass boost on,
      beta 0.2, gamma 0.5, LPC correction on. bass_boost only acts when
      post_filter is on. They never change the bitstream.
    */
    typedef struct codec2_model_opts
    {
        int post_filter;    /* LPC post filter on/off */
        int bass_boost;     /* +3 dB (amplitude 1.4x) below 1 kHz, post filter only */
        float beta;         /* post filter strength, default 0.2 */
        float gamma;        /* post filter bandwidth expansion, default 0.5 */
        int lpc_correction; /* attenuate A[1] when Wo < 150 Hz */
    } codec2_model_opts;

    void codec2_model_opts_default(codec2_model_opts *o);

    /* LSP -> LPC -> harmonic amplitudes (post filter, bass boost) and the
       first harmonic LPC correction for both halves. m[i] gets Wo, L,
       voicing and A[1..L] (other A[] zero), Aw[i] the FFT of A(z) that
       synthesis samples for the phases. opts == NULL means the defaults.
       Uses the decoder only as FFT scratch; its history is not touched. */
    void codec2_params_to_model(
        codec2_decoder_t *c2,
        const codec2_half_params p[2],
        const codec2_model_opts *opts,
        codec2_model_t m[2],
        codec2_complex_t Aw[2][CODEC2_AW_BINS]);

    /* Synthesis layer: phases, post filter phases, inverse FFT and
       overlap-add for both halves; updates the synthesis state and the
       phase generator. (Before C23, passing a non-const Aw array needs a
       cast to const codec2_complex_t (*)[CODEC2_AW_BINS] under -Wpedantic.) */
    void codec2_synthesise(
        codec2_decoder_t *c2,
        const codec2_model_t m[2],
        const codec2_complex_t Aw[2][CODEC2_AW_BINS],
        int16_t speech[CODEC2_SAMPLES_PER_FRAME]);

    /*
      Encoder-side helpers for callers that build the parameters themselves
      (no PCM analysis). The 3200 encoder does, per 20 ms frame:
          R[0..10]  autocorrelation of the windowed speech
          codec2_levinson_durbin(R, ak);
          e = sum(ak[i] * R[i], i = 0..10);   (before bandwidth expansion)
          codec2_lpc_bw_expand(ak);           (15 Hz expansion)
          codec2_lpc_to_lsp(ak, lsp);
          p1 = {Wo, L, v1, e, lsp} of the second 10 ms half
          codec2_pack_params(bits, v0, &p1);
      with v0 the voicing of the first half.
    */

    /* order 10 Levinson-Durbin, same math as the encoder. Returns the number
       of unstable reflection coefficients that were replaced by 0, or -1 if
       R[0] <= 0 (ak is then 1, 0, ..., 0). */
    int codec2_levinson_durbin(
        const float R[CODEC2_LPC_ORDER + 1],
        float ak[CODEC2_LPC_ORDER + 1]);

    /* the encoder's 15 Hz bandwidth expansion: ak[i] *= 0.994^i (table) */
    void codec2_lpc_bw_expand(float ak[CODEC2_LPC_ORDER + 1]);

    /* LPC -> LSP (radians) with the encoder's Chebyshev root search (grid
       step 0.01, 6 bisections). Returns the number of roots found; if it is
       not CODEC2_LPC_ORDER, lsp[] is set to the encoder's fallback values
       (i * pi / 10), as codec2_encode() does. */
    int codec2_lpc_to_lsp(
        const float ak[CODEC2_LPC_ORDER + 1],
        float lsp[CODEC2_LPC_ORDER]);

    /* Quantise and pack one frame exactly as codec2_encode() does after
       analysis: v0 (1 bit), p1->voiced (1 bit), p1->Wo (7 bits),
       p1->e (5 bits), 10 scalar LSP deltas of p1->lsp (5 bits each),
       Gray coded, MSB first. p1 is the second 10 ms half (Wo in
       rad/sample, e = linear LPC energy, lsp in radians; L is not used),
       v0 the voicing of the first half. */
    void codec2_pack_params(
        uint8_t bits[CODEC2_BYTES_PER_FRAME],
        int v0,
        const codec2_half_params *p1);

    /* Wo range of the 7-bit quantiser in rad/sample (CODEC2_WO_MIN and
       CODEC2_WO_MAX as float). Either pointer may be NULL. */
    void codec2_wo_limits(float *wo_min, float *wo_max);

    /* Set the decoder's phase generator (unvoiced and post filter phases).
       codec2_decoder_init() sets it to 1. Only the generator is changed. */
    void codec2_decoder_reseed(codec2_decoder_t *c2, uint32_t seed);

    /* library version string, e.g. "codec2-mod 3200 vocoder-api 2" */
    const char *codec2_mod_version(void);

    /* state sizes for opaque allocation */
    size_t codec2_size(void);
    size_t codec2_encoder_size(void);
    size_t codec2_decoder_size(void);
    size_t codec2_state_alignment(void);

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_MOD_H */
