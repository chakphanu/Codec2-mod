#ifndef CODEC2_MOD_LPC_H
#define CODEC2_MOD_LPC_H

#include "codec2_internal.h"

void aks_to_mag2(
    codec2_decoder_t *c2,
    const float *ak,
    model_t *model,
    float E,
    complex_t *Aw,
    float *A2);

/* aks_to_mag2() with the post filter options (aks_to_mag2() uses
   post_filter 1, bass_boost 1, LPCPF_BETA, LPCPF_GAMMA) */
void aks_to_mag2_opts(
    codec2_decoder_t *c2,
    const float *ak,
    model_t *model,
    float E,
    int post_filter,
    int bass_boost,
    float beta,
    float gamma,
    complex_t *Aw,
    float *A2);

float speech_to_uq_lsps(
    codec2_encoder_t *c2,
    float *restrict lsp,
    float *restrict ak,
    float *restrict energy,
    const float *restrict Sn,
    const float *restrict w);

void lsp_to_lpc(
    const float *restrict lsp,
    float *restrict ak);

void apply_lpc_correction(model_t *model);

#endif /* CODEC2_MOD_LPC_H */
