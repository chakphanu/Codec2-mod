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
      Parameter layer (shared with Codec2-mod-neural): the parameter half
      of codec2_decode(), usable on its own, e.g. by a caller that only
      needs the decoded parameters (a neural vocoder front end) with its
      own codec2_params_state. The decoder keeps its history in
      c2->params.
    */

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

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_MOD_H */
