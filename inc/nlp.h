#ifndef CODEC2_MOD_NLP_H
#define CODEC2_MOD_NLP_H

#include "codec2_internal.h"

/*
 * Non-Linear Pitch (NLP) estimator
 *
 * @param c2       Codec state (contains nlp_t and DSP backend)
 * @param Sn       Input speech vector
 * @param pitch    Estimated pitch period in samples at current Fs
 * @param prev_f0  Previous pitch f0 in Hz, memory for pitch tracking
 * @return         Best f0 estimate in Hz
 */
float nlp(
    codec2_t *c2,
    const float *restrict Sn,
    float *restrict pitch,
    float *restrict prev_f0
);

void nlp_init(nlp_t *nlp);

#endif /* CODEC2_MOD_NLP_H */
