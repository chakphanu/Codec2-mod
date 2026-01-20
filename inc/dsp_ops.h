/*
 * DSP Operations Interface for Codec2-mod
 *
 * This header defines an abstract interface for DSP primitives used by Codec2.
 * The codec core calls these operations via function pointers, allowing
 * different backends (reference C, SIMD, etc.) to be swapped at runtime.
 *
 * Design principles:
 * - No platform-specific headers in this file
 * - No #ifdef logic in codec algorithm files
 * - Backend selection via function pointer injection
 * - All backends must produce bit-exact results for correctness
 */

#ifndef DSP_OPS_H
#define DSP_OPS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Forward declaration - actual definition in codec2_internal.h
 * This avoids circular dependency
 */
struct codec2_t;

/*
 * DSP Operations Structure
 *
 * Each backend implements these function pointers.
 * The codec core accesses DSP operations via: c2->dsp->fft_forward(...)
 */
typedef struct dsp_ops {
    /*
     * Backend identifier string (for debugging/logging)
     */
    const char *name;

    /*
     * Initialize backend-specific resources
     * Called once during codec2_init()
     *
     * @param c2 Codec state (for accessing scratch buffers)
     * @return 0 on success, non-zero on error
     */
    int (*init)(struct codec2_t *c2);

    /*
     * Complex FFT (in-place)
     *
     * @param c2     Codec state
     * @param data   Complex array [re0, im0, re1, im1, ...] (n complex samples)
     * @param n      FFT size (must be power of 2)
     *
     * Note: Input/output format is interleaved complex floats
     */
    void (*fft_forward)(struct codec2_t *c2, float *data, int n);

    /*
     * Real-to-complex FFT
     *
     * @param c2     Codec state
     * @param input  Real input array (n samples)
     * @param output Complex output array (n/2+1 complex samples, interleaved)
     * @param n      FFT size
     */
    void (*fftr_forward)(struct codec2_t *c2, const float *input, float *output, int n);

    /*
     * Complex-to-real inverse FFT
     *
     * @param c2     Codec state
     * @param input  Complex input array (n/2+1 complex samples, interleaved)
     * @param output Real output array (n samples)
     * @param n      FFT size
     */
    void (*fftr_inverse)(struct codec2_t *c2, const float *input, float *output, int n);

    /*
     * Dot product (for autocorrelation)
     *
     * result = sum(a[i] * b[i]) for i = 0..len-1
     *
     * @param a      First input array
     * @param b      Second input array
     * @param result Pointer to store result
     * @param len    Number of elements
     */
    void (*dotprod)(const float *a, const float *b, float *result, int len);

    /*
     * Element-wise vector multiply
     *
     * output[i] = a[i] * b[i] for i = 0..len-1
     *
     * @param a      First input array
     * @param b      Second input array
     * @param output Output array (can alias a or b)
     * @param len    Number of elements
     */
    void (*vmul)(const float *a, const float *b, float *output, int len);

} dsp_ops_t;

/*
 * Get reference (pure C) DSP backend
 * Uses kiss_fft for FFT operations
 */
const dsp_ops_t *dsp_ops_ref(void);

/*
 * Get ESP-DSP SIMD backend (ESP32-S3 only)
 * Uses esp-dsp library for accelerated operations
 *
 * Returns NULL if not available on current platform
 */
const dsp_ops_t *dsp_ops_espdsp(void);

#ifdef __cplusplus
}
#endif

#endif /* DSP_OPS_H */
