#ifndef CODEC2_MOD_H
#define CODEC2_MOD_H

/*
  Public API of Codec2-mod (Codec2 3200 bps).

  This header includes codec2_internal.h, exactly as on main, so the codec
  state structs are complete types and the internal configuration macros
  (LPC_ORD, TWO_PI, ...), model_t and complex_t are visible. Code that uses
  the exported internals (quantise.h, interp.h, lpc.h) keeps building
  unchanged.

  The public types and sizes (codec2_model_t == model_t,
  codec2_complex_t == complex_t, CODEC2_* constants) come from
  codec2_mod_types.h, which the internal headers include as well, so
  every type has one definition.
*/

#include "codec2_mod_types.h"
#include "codec2_internal.h"

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

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_MOD_H */
