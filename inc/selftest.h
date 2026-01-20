/*
 * Codec2-mod Self-Test Framework
 *
 * Provides deterministic testing of DSP backend operations.
 * Used to verify SIMD backend produces bit-exact results with reference backend.
 */

#ifndef SELFTEST_H
#define SELFTEST_H

#include "codec2_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Self-test result codes
 */
typedef enum {
    SELFTEST_PASS = 0,
    SELFTEST_FAIL_FFT = 1,
    SELFTEST_FAIL_FFTR = 2,
    SELFTEST_FAIL_FFTRI = 3,
    SELFTEST_FAIL_DOTPROD = 4,
    SELFTEST_FAIL_VMUL = 5,
    SELFTEST_FAIL_ENCODE = 6,
    SELFTEST_FAIL_DECODE = 7
} selftest_result_t;

/*
 * Run self-test on a DSP backend
 *
 * Tests all DSP operations with known inputs and compares against
 * expected outputs. Uses deterministic test vectors.
 *
 * @param dsp   DSP backend to test (NULL uses reference backend)
 * @return      SELFTEST_PASS (0) on success, error code on failure
 */
selftest_result_t selftest_dsp(const dsp_ops_t *dsp);

/*
 * Compare two DSP backends for bit-exact equivalence
 *
 * Runs both backends with identical inputs and compares outputs.
 * Used to verify SIMD backend matches reference implementation.
 *
 * @param ref   Reference backend (typically dsp_ops_ref())
 * @param test  Backend under test (e.g., dsp_ops_espdsp())
 * @param tolerance  Maximum allowed difference (0.0 for bit-exact)
 * @return      SELFTEST_PASS (0) if backends match, error code otherwise
 */
selftest_result_t selftest_compare_backends(
    const dsp_ops_t *ref,
    const dsp_ops_t *test,
    float tolerance
);

/*
 * Full codec encode/decode round-trip test
 *
 * Encodes test audio, decodes it, and verifies output quality.
 * Uses deterministic synthetic test signal.
 *
 * @param dsp   DSP backend to test
 * @return      SELFTEST_PASS (0) on success, error code on failure
 */
selftest_result_t selftest_codec_roundtrip(const dsp_ops_t *dsp);

/*
 * Print self-test result as ASCII
 *
 * @param result  Result code from self-test function
 * @return        Human-readable string (static, do not free)
 */
const char *selftest_result_str(selftest_result_t result);

#ifdef __cplusplus
}
#endif

#endif /* SELFTEST_H */
