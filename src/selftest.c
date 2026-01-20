/*
 * Codec2-mod Self-Test Implementation
 *
 * Deterministic tests for DSP backend verification.
 * All test vectors are computed at compile time or use simple formulas.
 */

#include "selftest.h"
#include "codec2_mod.h"
#include <string.h>
#include <math.h>

/* Test vector size - must match FFT_ENC (512) as DSP backend uses fixed size */
#define TEST_SIZE FFT_ENC

/* Tolerance for floating-point comparison */
#define DEFAULT_TOLERANCE 1e-5f

/*
 * Generate deterministic test signal
 * Uses simple formula: x[i] = sin(2*pi*i*freq/size) * amplitude
 */
static void generate_test_signal(float *out, int size, float freq, float amp)
{
    for (int i = 0; i < size; i++) {
        out[i] = amp * sinf(TWO_PI * i * freq / size);
    }
}

/*
 * Compare two float arrays within tolerance
 * Returns 0 if all elements match, 1 otherwise
 */
static int compare_arrays(const float *a, const float *b, int len, float tol)
{
    for (int i = 0; i < len; i++) {
        float diff = fabsf(a[i] - b[i]);
        if (diff > tol) {
            return 1; /* mismatch */
        }
    }
    return 0; /* match */
}

/*
 * Test complex FFT forward
 */
static int test_fft_forward(codec2_t *c2, const dsp_ops_t *dsp, float tol)
{
    /* Use static buffers to avoid stack overflow (each is 4KB) */
    static float input[TEST_SIZE * 2] __attribute__((aligned(16)));
    static float output[TEST_SIZE * 2] __attribute__((aligned(16)));
    static float expected[TEST_SIZE * 2] __attribute__((aligned(16)));

    /* Generate test input - simple impulse */
    memset(input, 0, sizeof(input));
    input[0] = 1.0f; /* DC impulse - FFT should give all 1s for real */

    /* Copy for in-place operation */
    memcpy(output, input, sizeof(output));

    /* Run FFT */
    dsp->fft_forward(c2, output, TEST_SIZE);

    /* Verify: FFT of impulse at DC gives constant spectrum */
    /* output[2*i] should be 1.0, output[2*i+1] should be 0.0 */
    for (int i = 0; i < TEST_SIZE; i++) {
        expected[2*i] = 1.0f;     /* real */
        expected[2*i + 1] = 0.0f; /* imag */
    }

    return compare_arrays(output, expected, TEST_SIZE * 2, tol);
}

/*
 * Test real-to-complex FFT
 */
static int test_fftr_forward(codec2_t *c2, const dsp_ops_t *dsp, float tol)
{
    /* Use static buffers to avoid stack overflow */
    static float input[TEST_SIZE] __attribute__((aligned(16)));
    static float output[(TEST_SIZE / 2 + 1) * 2] __attribute__((aligned(16)));

    /* Generate sine wave test signal */
    generate_test_signal(input, TEST_SIZE, 4.0f, 1.0f);

    /* Run real FFT */
    dsp->fftr_forward(c2, input, output, TEST_SIZE);

    /* For a sine wave at bin 4, we expect:
     * - Large magnitude at bin 4
     * - Small magnitudes elsewhere (numerical noise)
     */
    float mag_at_4 = sqrtf(output[4*2] * output[4*2] + output[4*2+1] * output[4*2+1]);
    float mag_at_0 = sqrtf(output[0] * output[0] + output[1] * output[1]);

    /* Sine at bin 4 should dominate */
    if (mag_at_4 < mag_at_0 * 10.0f) {
        return 1; /* fail */
    }

    return 0; /* pass */
}

/*
 * Test inverse real FFT (round-trip)
 *
 * Note: kiss_fft does NOT normalize. The round-trip FFT->IFFT
 * produces output scaled by N. We must scale before comparison.
 */
static int test_fftr_inverse(codec2_t *c2, const dsp_ops_t *dsp, float tol)
{
    /* Use static buffers to avoid stack overflow */
    static float original[TEST_SIZE] __attribute__((aligned(16)));
    static float freq[(TEST_SIZE / 2 + 1) * 2] __attribute__((aligned(16)));
    static float recovered[TEST_SIZE] __attribute__((aligned(16)));

    /* Generate test signal */
    generate_test_signal(original, TEST_SIZE, 3.0f, 0.5f);

    /* Forward FFT */
    dsp->fftr_forward(c2, original, freq, TEST_SIZE);

    /* Inverse FFT */
    dsp->fftr_inverse(c2, freq, recovered, TEST_SIZE);

    /* kiss_fft round-trip is scaled by N, so normalize before comparison */
    float scale = 1.0f / (float)TEST_SIZE;
    for (int i = 0; i < TEST_SIZE; i++) {
        recovered[i] *= scale;
    }

    /* Compare - should match original (within tolerance) */
    return compare_arrays(original, recovered, TEST_SIZE, tol);
}

/*
 * Test dot product
 */
static int test_dotprod(const dsp_ops_t *dsp, float tol)
{
    /* Use static buffers to avoid stack overflow */
    static float a[TEST_SIZE] __attribute__((aligned(16)));
    static float b[TEST_SIZE] __attribute__((aligned(16)));
    float result;

    /* Simple test: a[i] = i, b[i] = 1 -> result = sum(0..N-1) = N*(N-1)/2 */
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)i;
        b[i] = 1.0f;
    }

    dsp->dotprod(a, b, &result, TEST_SIZE);

    float expected = (float)(TEST_SIZE * (TEST_SIZE - 1)) / 2.0f;

    if (fabsf(result - expected) > tol) {
        return 1; /* fail */
    }

    return 0; /* pass */
}

/*
 * Test vector multiply
 */
static int test_vmul(const dsp_ops_t *dsp, float tol)
{
    /* Use static buffers to avoid stack overflow */
    static float a[TEST_SIZE] __attribute__((aligned(16)));
    static float b[TEST_SIZE] __attribute__((aligned(16)));
    static float output[TEST_SIZE] __attribute__((aligned(16)));
    static float expected[TEST_SIZE] __attribute__((aligned(16)));

    /* Test: a[i] = i+1, b[i] = 2 -> output[i] = 2*(i+1) */
    for (int i = 0; i < TEST_SIZE; i++) {
        a[i] = (float)(i + 1);
        b[i] = 2.0f;
        expected[i] = 2.0f * (i + 1);
    }

    dsp->vmul(a, b, output, TEST_SIZE);

    return compare_arrays(output, expected, TEST_SIZE, tol);
}

/*
 * Run self-test on a DSP backend
 */
selftest_result_t selftest_dsp(const dsp_ops_t *dsp)
{
    /* Use static allocation - codec2_t is too large for stack (~25KB) */
    static codec2_t c2;
    const dsp_ops_t *backend = dsp ? dsp : dsp_ops_ref();

    /* Initialize codec with specified backend */
    if (codec2_init_with_dsp(&c2, backend) != 0) {
        return SELFTEST_FAIL_FFT; /* init failure */
    }

    /* Run tests */
    if (test_fft_forward(&c2, backend, DEFAULT_TOLERANCE) != 0) {
        return SELFTEST_FAIL_FFT;
    }

    if (test_fftr_forward(&c2, backend, DEFAULT_TOLERANCE) != 0) {
        return SELFTEST_FAIL_FFTR;
    }

    if (test_fftr_inverse(&c2, backend, DEFAULT_TOLERANCE) != 0) {
        return SELFTEST_FAIL_FFTRI;
    }

    if (test_dotprod(backend, DEFAULT_TOLERANCE) != 0) {
        return SELFTEST_FAIL_DOTPROD;
    }

    if (test_vmul(backend, DEFAULT_TOLERANCE) != 0) {
        return SELFTEST_FAIL_VMUL;
    }

    return SELFTEST_PASS;
}

/*
 * Compare two DSP backends for equivalence
 */
selftest_result_t selftest_compare_backends(
    const dsp_ops_t *ref,
    const dsp_ops_t *test,
    float tolerance
)
{
    /* Use static allocation - codec2_t is too large for stack (~25KB each) */
    static codec2_t c2_ref, c2_test;
    float tol = (tolerance > 0.0f) ? tolerance : DEFAULT_TOLERANCE;

    /* Initialize both codecs */
    if (codec2_init_with_dsp(&c2_ref, ref) != 0) {
        return SELFTEST_FAIL_FFT;
    }
    if (codec2_init_with_dsp(&c2_test, test) != 0) {
        return SELFTEST_FAIL_FFT;
    }

    /* Use static buffers to avoid stack overflow */
    static float fft_input[TEST_SIZE * 2] __attribute__((aligned(16)));
    static float fft_out_ref[TEST_SIZE * 2] __attribute__((aligned(16)));
    static float fft_out_test[TEST_SIZE * 2] __attribute__((aligned(16)));
    static float fftr_input[TEST_SIZE] __attribute__((aligned(16)));
    static float fftr_out_ref[(TEST_SIZE / 2 + 1) * 2] __attribute__((aligned(16)));
    static float fftr_out_test[(TEST_SIZE / 2 + 1) * 2] __attribute__((aligned(16)));
    static float freq[(TEST_SIZE / 2 + 1) * 2] __attribute__((aligned(16)));
    static float ifft_out_ref[TEST_SIZE] __attribute__((aligned(16)));
    static float ifft_out_test[TEST_SIZE] __attribute__((aligned(16)));
    static float dot_a[TEST_SIZE] __attribute__((aligned(16)));
    static float dot_b[TEST_SIZE] __attribute__((aligned(16)));
    static float vmul_a[TEST_SIZE] __attribute__((aligned(16)));
    static float vmul_b[TEST_SIZE] __attribute__((aligned(16)));
    static float vmul_out_ref[TEST_SIZE] __attribute__((aligned(16)));
    static float vmul_out_test[TEST_SIZE] __attribute__((aligned(16)));

    /* Test FFT forward */
    {
        generate_test_signal(fft_input, TEST_SIZE * 2, 5.0f, 0.8f);
        memcpy(fft_out_ref, fft_input, sizeof(fft_out_ref));
        memcpy(fft_out_test, fft_input, sizeof(fft_out_test));

        ref->fft_forward(&c2_ref, fft_out_ref, TEST_SIZE);
        test->fft_forward(&c2_test, fft_out_test, TEST_SIZE);

        if (compare_arrays(fft_out_ref, fft_out_test, TEST_SIZE * 2, tol) != 0) {
            return SELFTEST_FAIL_FFT;
        }
    }

    /* Test real FFT forward */
    {
        generate_test_signal(fftr_input, TEST_SIZE, 7.0f, 0.6f);

        ref->fftr_forward(&c2_ref, fftr_input, fftr_out_ref, TEST_SIZE);
        test->fftr_forward(&c2_test, fftr_input, fftr_out_test, TEST_SIZE);

        if (compare_arrays(fftr_out_ref, fftr_out_test, (TEST_SIZE / 2 + 1) * 2, tol) != 0) {
            return SELFTEST_FAIL_FFTR;
        }
    }

    /* Test inverse real FFT */
    {
        /* Create frequency domain data */
        memset(freq, 0, sizeof(freq));
        freq[2*3] = 1.0f; /* bin 3 real */
        freq[2*3 + 1] = 0.5f; /* bin 3 imag */

        ref->fftr_inverse(&c2_ref, freq, ifft_out_ref, TEST_SIZE);
        test->fftr_inverse(&c2_test, freq, ifft_out_test, TEST_SIZE);

        if (compare_arrays(ifft_out_ref, ifft_out_test, TEST_SIZE, tol) != 0) {
            return SELFTEST_FAIL_FFTRI;
        }
    }

    /* Test dot product */
    {
        float result_ref, result_test;

        generate_test_signal(dot_a, TEST_SIZE, 2.0f, 1.0f);
        generate_test_signal(dot_b, TEST_SIZE, 3.0f, 1.0f);

        ref->dotprod(dot_a, dot_b, &result_ref, TEST_SIZE);
        test->dotprod(dot_a, dot_b, &result_test, TEST_SIZE);

        if (fabsf(result_ref - result_test) > tol) {
            return SELFTEST_FAIL_DOTPROD;
        }
    }

    /* Test vmul */
    {
        generate_test_signal(vmul_a, TEST_SIZE, 4.0f, 0.9f);
        generate_test_signal(vmul_b, TEST_SIZE, 6.0f, 0.7f);

        ref->vmul(vmul_a, vmul_b, vmul_out_ref, TEST_SIZE);
        test->vmul(vmul_a, vmul_b, vmul_out_test, TEST_SIZE);

        if (compare_arrays(vmul_out_ref, vmul_out_test, TEST_SIZE, tol) != 0) {
            return SELFTEST_FAIL_VMUL;
        }
    }

    return SELFTEST_PASS;
}

/*
 * Full codec round-trip test
 */
selftest_result_t selftest_codec_roundtrip(const dsp_ops_t *dsp)
{
    /* Use static allocation - codec2_t is too large for stack (~25KB) */
    static codec2_t c2;
    const dsp_ops_t *backend = dsp ? dsp : dsp_ops_ref();

    int16_t input[SAMPLES_PER_FRAME];
    int16_t output[SAMPLES_PER_FRAME];
    uint8_t bits[BYTES_PER_FRAME];

    /* Initialize codec */
    if (codec2_init_with_dsp(&c2, backend) != 0) {
        return SELFTEST_FAIL_ENCODE;
    }

    /* Generate test audio - 400 Hz tone at 8kHz sample rate */
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        float sample = 16000.0f * sinf(TWO_PI * 400.0f * i / SAMP_RATE);
        input[i] = (int16_t)sample;
    }

    /* Encode */
    codec2_encode(&c2, bits, input);

    /* Re-init for clean decode state */
    if (codec2_init_with_dsp(&c2, backend) != 0) {
        return SELFTEST_FAIL_DECODE;
    }

    /* Decode */
    codec2_decode(&c2, output, bits);

    /* Basic sanity check - output should have non-zero energy */
    int32_t energy = 0;
    for (int i = 0; i < SAMPLES_PER_FRAME; i++) {
        energy += (int32_t)output[i] * output[i];
    }

    if (energy < 1000) {
        return SELFTEST_FAIL_DECODE; /* output too quiet */
    }

    return SELFTEST_PASS;
}

/*
 * Get human-readable result string
 */
const char *selftest_result_str(selftest_result_t result)
{
    switch (result) {
        case SELFTEST_PASS:        return "PASS";
        case SELFTEST_FAIL_FFT:    return "FAIL: FFT";
        case SELFTEST_FAIL_FFTR:   return "FAIL: Real FFT";
        case SELFTEST_FAIL_FFTRI:  return "FAIL: Inverse Real FFT";
        case SELFTEST_FAIL_DOTPROD:return "FAIL: Dot Product";
        case SELFTEST_FAIL_VMUL:   return "FAIL: Vector Multiply";
        case SELFTEST_FAIL_ENCODE: return "FAIL: Encode";
        case SELFTEST_FAIL_DECODE: return "FAIL: Decode";
        default:                   return "FAIL: Unknown";
    }
}
