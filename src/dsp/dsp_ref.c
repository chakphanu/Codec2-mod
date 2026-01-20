/*
 * Reference DSP Backend for Codec2-mod
 *
 * Pure C implementation using kiss_fft for FFT operations.
 * This serves as the reference implementation for correctness verification.
 */

#include "dsp_ops.h"
#include "codec2_internal.h"
#include "kiss_fft.h"
#include "kiss_fftr.h"
#include <string.h>

/*
 * Initialize kiss_fft configurations
 * These are stored in codec2_t for static allocation
 */
static int ref_init(struct codec2_t *c2)
{
    size_t mem;

    /* Complex FFT forward */
    mem = sizeof(c2->fft_fwd_mem);
    c2->fft_fwd_cfg = kiss_fft_alloc(FFT_ENC, 0, c2->fft_fwd_mem, &mem);
    if (c2->fft_fwd_cfg == NULL) return -1;

    /* Real FFT forward */
    mem = sizeof(c2->fftr_fwd_mem);
    c2->fftr_fwd_cfg = kiss_fftr_alloc(FFT_ENC, 0, c2->fftr_fwd_mem, &mem);
    if (c2->fftr_fwd_cfg == NULL) return -1;

    /* Real FFT inverse */
    mem = sizeof(c2->fftr_inv_mem);
    c2->fftr_inv_cfg = kiss_fftr_alloc(FFT_DEC, 1, c2->fftr_inv_mem, &mem);
    if (c2->fftr_inv_cfg == NULL) return -1;

    /* NLP shares the forward real FFT config */
    c2->nlp.fftr_cfg = c2->fftr_fwd_cfg;

    return 0;
}

/*
 * Complex FFT (in-place)
 *
 * kiss_fft uses struct { float r, i; } which is memory-compatible
 * with interleaved float format [re0, im0, re1, im1, ...]
 */
static void ref_fft_forward(struct codec2_t *c2, float *data, int n)
{
    (void)n; /* Size is fixed at FFT_ENC for this codec */
    kiss_fft_cpx *cpx = (kiss_fft_cpx *)data;
    kiss_fft(c2->fft_fwd_cfg, cpx, cpx);
}

/*
 * Real-to-complex FFT
 *
 * Input: n real samples
 * Output: n/2+1 complex samples in interleaved format
 */
static void ref_fftr_forward(struct codec2_t *c2, const float *input, float *output, int n)
{
    (void)n;
    kiss_fft_cpx *cpx_out = (kiss_fft_cpx *)output;
    kiss_fftr(c2->fftr_fwd_cfg, input, cpx_out);
}

/*
 * Complex-to-real inverse FFT
 *
 * Input: n/2+1 complex samples in interleaved format
 * Output: n real samples
 */
static void ref_fftr_inverse(struct codec2_t *c2, const float *input, float *output, int n)
{
    (void)n;
    const kiss_fft_cpx *cpx_in = (const kiss_fft_cpx *)input;
    kiss_fftri(c2->fftr_inv_cfg, cpx_in, output);
}

/*
 * Scalar dot product
 */
static void ref_dotprod(const float *a, const float *b, float *result, int len)
{
    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    *result = sum;
}

/*
 * Scalar element-wise multiply
 */
static void ref_vmul(const float *a, const float *b, float *output, int len)
{
    for (int i = 0; i < len; i++) {
        output[i] = a[i] * b[i];
    }
}

/*
 * Reference backend instance
 */
static const dsp_ops_t ref_ops = {
    .name         = "reference (kiss_fft)",
    .init         = ref_init,
    .fft_forward  = ref_fft_forward,
    .fftr_forward = ref_fftr_forward,
    .fftr_inverse = ref_fftr_inverse,
    .dotprod      = ref_dotprod,
    .vmul         = ref_vmul,
};

const dsp_ops_t *dsp_ops_ref(void)
{
    return &ref_ops;
}
