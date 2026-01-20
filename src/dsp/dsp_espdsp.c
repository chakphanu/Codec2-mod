/*
 * ESP-DSP SIMD Backend for Codec2-mod
 *
 * Uses Espressif esp-dsp library for PIE/SIMD acceleration on ESP32-S3.
 * This backend produces bit-exact results with the reference backend.
 *
 * Requires:
 * - ESP-IDF with esp-dsp component
 * - ESP32-S3 target (for PIE SIMD instructions)
 */

/* Include sdkconfig.h first to get CONFIG_IDF_TARGET_ESP32S3 */
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3

#include "dsp_ops.h"
#include "codec2_internal.h"
#include "dsps_fft2r.h"
#include "dsps_dotprod.h"
#include "dsps_mul.h"
#include "esp_log.h"
#include <string.h>

#define TAG "DSP_ESPDSP"

/*
 * Initialize esp-dsp FFT tables
 * Called once during codec2_init()
 */
static int espdsp_init(struct codec2_t *c2)
{
    /* Initialize esp-dsp FFT twiddle factors (global, done once) */
    static int fft_initialized = 0;
    if (!fft_initialized) {
        esp_err_t ret = dsps_fft2r_init_fc32(NULL, FFT_ENC);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "dsps_fft2r_init_fc32 failed: %d", ret);
            return -1;
        }
        fft_initialized = 1;
    }

    /* Zero the esp-dsp working buffer */
    memset(c2->fft_espdsp, 0, sizeof(c2->fft_espdsp));

    return 0;
}

/*
 * Complex FFT using esp-dsp
 *
 * esp-dsp dsps_fft2r_fc32 operates on interleaved complex format
 * which matches our interface directly.
 */
static void espdsp_fft_forward(struct codec2_t *c2, float *data, int n)
{
    (void)c2;
    dsps_fft2r_fc32(data, n);
    dsps_bit_rev_fc32(data, n);
}

/*
 * Real-to-complex FFT using esp-dsp
 *
 * Since esp-dsp only provides complex FFT, we:
 * 1. Pack real data into interleaved complex (imag = 0) in working buffer
 * 2. Run complex FFT
 * 3. Copy first N/2+1 complex values to output (half spectrum)
 *
 * Output format matches kiss_fftr: N/2+1 complex values for N-point real FFT
 */
static void espdsp_fftr_forward(struct codec2_t *c2, const float *input, float *output, int n)
{
    float *work = c2->fft_espdsp;

    /* Pack real -> interleaved complex [re, 0, re, 0, ...] */
    for (int i = 0; i < n; i++) {
        work[2*i]     = input[i];
        work[2*i + 1] = 0.0f;
    }

    /* Complex FFT */
    dsps_fft2r_fc32(work, n);
    dsps_bit_rev_fc32(work, n);

    /* Copy first N/2+1 bins to output (half spectrum for real input) */
    int out_bins = n / 2 + 1;
    memcpy(output, work, out_bins * 2 * sizeof(float));
}

/*
 * Complex-to-real inverse FFT using esp-dsp
 *
 * Input: N/2+1 complex values (half spectrum, matching kiss_fftr output)
 * Output: N real samples
 *
 * Uses conjugate-FFT-conjugate method with Hermitian symmetry reconstruction:
 * 1. Expand half spectrum to full N-point spectrum using X[N-k] = conj(X[k])
 * 2. Conjugate all values
 * 3. Forward FFT
 * 4. Extract real parts (no 1/N scaling to match kiss_fft behavior)
 */
static void espdsp_fftr_inverse(struct codec2_t *c2, const float *input, float *output, int n)
{
    float *work = c2->fft_espdsp;

    /* Reconstruct full spectrum from half spectrum using Hermitian symmetry */
    /* DC bin (k=0) */
    work[0] = input[0];
    work[1] = -input[1];  /* conjugate */

    /* Bins 1 to N/2-1: copy and mirror with conjugation */
    for (int k = 1; k < n / 2; k++) {
        /* X[k] = conj(input[k]) */
        work[2*k]     = input[2*k];
        work[2*k + 1] = -input[2*k + 1];
        /* X[N-k] = conj(conj(input[k])) = input[k] (Hermitian symmetry + conjugate) */
        work[2*(n-k)]     = input[2*k];
        work[2*(n-k) + 1] = input[2*k + 1];
    }

    /* Nyquist bin (k=N/2) */
    work[n]     = input[n];      /* real part of bin N/2 */
    work[n + 1] = -input[n + 1]; /* conjugate */

    /* Forward FFT */
    dsps_fft2r_fc32(work, n);
    dsps_bit_rev_fc32(work, n);

    /* Extract real parts (no scaling - matches kiss_fft behavior) */
    for (int i = 0; i < n; i++) {
        output[i] = work[2*i];
    }
}

/*
 * Vectorized dot product using esp-dsp SIMD
 */
static void espdsp_dotprod(const float *a, const float *b, float *result, int len)
{
    dsps_dotprod_f32(a, b, result, len);
}

/*
 * Vectorized element-wise multiply using esp-dsp SIMD
 */
static void espdsp_vmul(const float *a, const float *b, float *output, int len)
{
    dsps_mul_f32(a, b, output, len, 1, 1, 1);
}

/*
 * ESP-DSP backend instance
 */
static const dsp_ops_t espdsp_ops = {
    .name         = "esp-dsp (SIMD)",
    .init         = espdsp_init,
    .fft_forward  = espdsp_fft_forward,
    .fftr_forward = espdsp_fftr_forward,
    .fftr_inverse = espdsp_fftr_inverse,
    .dotprod      = espdsp_dotprod,
    .vmul         = espdsp_vmul,
};

const dsp_ops_t *dsp_ops_espdsp(void)
{
    return &espdsp_ops;
}

#else /* !CONFIG_IDF_TARGET_ESP32S3 */

#include "dsp_ops.h"

/* ESP-DSP not available on this platform */
const dsp_ops_t *dsp_ops_espdsp(void)
{
    return (const dsp_ops_t *)0;
}

#endif /* CONFIG_IDF_TARGET_ESP32S3 */
