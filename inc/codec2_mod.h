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

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_MOD_H */
