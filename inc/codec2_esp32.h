/*
 * ESP32-S3 Platform Abstraction for Codec2-mod
 *
 * This header provides:
 * - 16-byte alignment macros for SIMD operations
 * - esp-dsp function wrappers
 * - Memory placement attributes for internal SRAM
 *
 * When CODEC2_ESP32S3_DSP is defined:
 * - FFT operations use dsps_fft2r_fc32 (PIE SIMD accelerated)
 * - Dot products use dsps_dotprod_f32 (vectorized)
 *
 * When not defined:
 * - Falls back to kiss_fft (portable C)
 */

#ifndef CODEC2_ESP32_H
#define CODEC2_ESP32_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Alignment macro for 16-byte boundary
 * Required for ESP32-S3 PIE SIMD instructions (128-bit vectors)
 */
#ifdef CODEC2_ESP32S3_DSP
    #define CODEC2_ALIGNED __attribute__((aligned(16)))
    #define CODEC2_SRAM_ATTR __attribute__((section(".dram1.data")))
#else
    #define CODEC2_ALIGNED
    #define CODEC2_SRAM_ATTR
#endif

/*
 * ESP-DSP includes and initialization
 */
#ifdef CODEC2_ESP32S3_DSP

#include "dsps_fft2r.h"
#include "dsps_dotprod.h"
#include "dsps_fir.h"
#include "dsps_mulc.h"
#include "dsps_mul.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <stdint.h>

/*
 * Debug helper: Check if a pointer is 16-byte aligned
 * Use this in debug builds to verify alignment requirements
 */
#define CODEC2_CHECK_ALIGNMENT(ptr) \
    do { \
        if (((uintptr_t)(ptr) & 0xF) != 0) { \
            ESP_LOGE("CODEC2", "Alignment error: %s at %p is not 16-byte aligned", #ptr, (void*)(ptr)); \
        } \
    } while(0)

/*
 * Debug helper: Verify buffer is in internal SRAM
 * PSRAM access is 12-24x slower and will bottleneck SIMD operations
 */
static inline void codec2_verify_sram_placement(const void *ptr, const char *name __attribute__((unused)))
{
    if (ptr == NULL) return;

    /* For statically allocated buffers, check if in DRAM range */
    uintptr_t addr = (uintptr_t)ptr;

    /* ESP32-S3 internal SRAM range (approximate) */
    /* DRAM: 0x3FC88000 - 0x3FCFFFFF (approx 512KB) */
    if (addr >= 0x3FC88000 && addr < 0x3FD00000) {
        ESP_LOGD("CODEC2", "%s: OK - in internal SRAM at %p", name, ptr);
    } else if (addr >= 0x3C000000 && addr < 0x3D000000) {
        ESP_LOGW("CODEC2", "%s: WARNING - in PSRAM at %p (slow for DSP!)", name, ptr);
    } else {
        ESP_LOGD("CODEC2", "%s: at %p (stack/other)", name, ptr);
    }
}

/*
 * FFT Configuration for esp-dsp
 * dsps_fft2r_fc32 operates on interleaved complex format:
 * [Re0, Im0, Re1, Im1, ...]
 */
#define CODEC2_FFT_INTERLEAVED 1

/*
 * Initialize esp-dsp FFT tables
 * Must be called once at startup before any FFT operations
 * Returns ESP_OK on success
 */
static inline esp_err_t codec2_espdsp_init(int max_fft_size)
{
    return dsps_fft2r_init_fc32(NULL, max_fft_size);
}

/*
 * Perform in-place forward FFT using esp-dsp
 *
 * @param data Interleaved complex buffer [Re0, Im0, Re1, Im1, ...]
 *             Must be 16-byte aligned
 *             Size must be 2*n floats
 * @param n FFT size (power of 2, 16-4096)
 *
 * Note: dsps_fft2r_fc32 performs in-place transform
 * Bit-reversal must be called after FFT
 */
static inline void codec2_fft_forward(float *data, int n)
{
    dsps_fft2r_fc32(data, n);
    dsps_bit_rev_fc32(data, n);
}

/*
 * Perform in-place inverse FFT using esp-dsp
 * Uses conjugate-FFT-conjugate method
 *
 * @param data Interleaved complex buffer
 * @param n FFT size
 */
static inline void codec2_fft_inverse(float *data, int n)
{
    /* Conjugate (negate imaginary parts) */
    for (int i = 0; i < n; i++) {
        data[2*i + 1] = -data[2*i + 1];
    }

    /* Forward FFT */
    dsps_fft2r_fc32(data, n);
    dsps_bit_rev_fc32(data, n);

    /* Conjugate and scale */
    float scale = 1.0f / (float)n;
    for (int i = 0; i < n; i++) {
        data[2*i] *= scale;
        data[2*i + 1] = -data[2*i + 1] * scale;
    }
}

/*
 * Real-to-complex FFT wrapper
 * Converts real input to interleaved complex and performs FFT
 *
 * @param input Real input array (n samples)
 * @param output Interleaved complex output (2*n floats, in-place capable)
 * @param n FFT size
 */
static inline void codec2_fftr_forward(const float *input, float *output, int n)
{
    /* Pack real data into interleaved complex format */
    for (int i = n - 1; i >= 0; i--) {
        output[2*i] = input[i];
        output[2*i + 1] = 0.0f;
    }

    /* Forward FFT */
    dsps_fft2r_fc32(output, n);
    dsps_bit_rev_fc32(output, n);
}

/*
 * Complex-to-real inverse FFT wrapper
 *
 * @param data Interleaved complex input (modified in-place)
 * @param output Real output array
 * @param n FFT size
 */
static inline void codec2_fftr_inverse(float *data, float *output, int n)
{
    codec2_fft_inverse(data, n);

    /* Extract real parts */
    for (int i = 0; i < n; i++) {
        output[i] = data[2*i];
    }
}

/*
 * Vectorized dot product using esp-dsp SIMD
 *
 * @param a First input array (should be aligned for best performance)
 * @param b Second input array
 * @param result Pointer to result
 * @param len Length of vectors
 */
static inline void codec2_dotprod(const float *a, const float *b, float *result, int len)
{
    dsps_dotprod_f32(a, b, result, len);
}

/*
 * Vectorized multiply-accumulate for autocorrelation
 *
 * @param x Input signal (aligned)
 * @param R Output autocorrelation coefficients
 * @param n Signal length
 * @param order LPC order (number of lags - 1)
 */
static inline void codec2_autocorrelate_simd(const float *x, float *R, int n, int order)
{
    for (int k = 0; k <= order; k++) {
        dsps_dotprod_f32(&x[k], &x[0], &R[k], n - k);
    }
}

/*
 * Vectorized element-wise multiply (for windowing)
 *
 * @param a Input array a
 * @param b Input array b
 * @param output Output array (can be same as a or b)
 * @param len Length of arrays
 */
static inline void codec2_vmul(const float *a, const float *b, float *output, int len)
{
    dsps_mul_f32(a, b, output, len, 1, 1, 1);
}

#else /* !CODEC2_ESP32S3_DSP - Fallback to kiss_fft */

#define CODEC2_FFT_INTERLEAVED 0

/* No-op initialization for non-ESP platforms */
static inline int codec2_espdsp_init(int max_fft_size)
{
    (void)max_fft_size;
    return 0;
}

/* Scalar dot product fallback */
static inline void codec2_dotprod(const float *a, const float *b, float *result, int len)
{
    float sum = 0.0f;
    for (int i = 0; i < len; i++) {
        sum += a[i] * b[i];
    }
    *result = sum;
}

/* Scalar autocorrelation fallback */
static inline void codec2_autocorrelate_simd(const float *x, float *R, int n, int order)
{
    for (int k = 0; k <= order; k++) {
        R[k] = 0.0f;
        for (int i = 0; i < n - k; i++) {
            R[k] += x[i] * x[i + k];
        }
    }
}

/* Scalar multiply fallback */
static inline void codec2_vmul(const float *a, const float *b, float *output, int len)
{
    for (int i = 0; i < len; i++) {
        output[i] = a[i] * b[i];
    }
}

#endif /* CODEC2_ESP32S3_DSP */

#ifdef __cplusplus
}
#endif

#endif /* CODEC2_ESP32_H */
