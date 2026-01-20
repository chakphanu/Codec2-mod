#ifndef CODEC2_MOD_H
#define CODEC2_MOD_H

#include "codec2_internal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct codec2_t codec2_t;

/* public frame sizes */
#define CODEC2_BYTES_PER_FRAME BYTES_PER_FRAME
#define CODEC2_SAMPLES_PER_FRAME SAMPLES_PER_FRAME

    /*
     * Initialize codec with specified DSP backend
     *
     * @param c2  Codec state structure (caller-allocated)
     * @param dsp DSP backend (from dsp_ops_ref() or dsp_ops_espdsp())
     * @return 0 on success, non-zero on error
     */
    int codec2_init_with_dsp(codec2_t *c2, const dsp_ops_t *dsp);

    /*
     * Initialize codec with default (reference) DSP backend
     * Convenience wrapper for codec2_init_with_dsp(c2, dsp_ops_ref())
     */
    void codec2_init(codec2_t *c2);

    void codec2_encode(
        codec2_t *c2,
        uint8_t *bits,
        const int16_t *speech);

    void codec2_decode(
        codec2_t *c2,
        int16_t *speech,
        const uint8_t *bits);

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_MOD_H */
