#ifndef CODEC2_MOD_INTERNAL_H
#define CODEC2_MOD_INTERNAL_H

/*
  This work uses code written by David Rowe VK5DGR et al.
  https://github.com/drowe67/codec2

  ESP32-S3 optimizations added for PIE/SIMD acceleration via esp-dsp
*/

#include <stdint.h>
#include <math.h>
#include <string.h>

#include "codec2_esp32.h"
#include "kiss_fft.h"
#include "kiss_fftr.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TWO_PI (2.0f * M_PI)

#ifndef POW10F
#define POW10F(x) powf(10.0f, (x))
#endif

#define BYTES_PER_FRAME 8
#define SAMPLES_PER_FRAME (2 * N_SAMP)

#define N_S 0.01
#define MAX_AMP 80

#define FFT_ENC 512
#define FFT_DEC 512
#define V_THRESH 6.0
#define V_THRESH_LIN (powf(10.0f, V_THRESH * 0.1f))
#define LPC_ORD 10

#define P_MAX_S 0.0200f

#define SAMP_RATE 8000
#define N_SAMP 80
#define M_PITCH 320
#define P_MIN 20
#define P_MAX 160
#define W0_MIN ((2.0 * M_PI) / 160.0)
#define W0_MAX ((2.0 * M_PI) / 20.0)
#define NW 279
#define TW 40

#define PMAX_M 320
#define COEFF 0.95
#define PE_FFT_SIZE 512
#define DEC 5
#define NDEC (M_PITCH / DEC)
#define T 0.1
#define CNLP 0.3
#define NLP_NTAP 48
#define NLP_NPHASE DEC
#define NLP_TAPS_PP ((NLP_NTAP + DEC - 1) / DEC)

#define WO_BITS 7
#define WO_LEVELS (1 << WO_BITS)
#define E_BITS 5
#define E_LEVELS (1 << E_BITS)
#define E_MIN_DB -10.0
#define E_MAX_DB 40.0
#define LSPD_SCALAR_INDEXES 10

#define LPCPF_GAMMA 0.5
#define LPCPF_BETA 0.2
#define LPCPF_TWO_BETA (2.0 * LPCPF_BETA)
#define LSP_DELTA1 0.01
#define BG_THRESH 40.0
#define BG_BETA 0.1
#define BG_MARGIN 6.0
#define LPC_ENERGY_FLOOR 1e-6f

#define FFT_R (TWO_PI / FFT_ENC)
#define FFT_1_R (FFT_ENC / TWO_PI)

#define CODEC2_RAND_MAX 32767

#define FFT_FWD_MEM_BYTES 4360
#define FFTR_MEM_BYTES 5408

/* asserts */
/* this is the exact Codec2 3200 bps configuration */
/* changing any of these parameters requires DSP math modification and buffer resizing */
_Static_assert(BYTES_PER_FRAME == 8, "Codec2 3200 only");
_Static_assert(LPC_ORD == 10, "Codec2 3200 assumes LPC_ORD == 10");
_Static_assert(MAX_AMP >= 80, "MAX_AMP must be >= 80 for Codec2 3200");
_Static_assert(FFT_ENC == 512, "Codec2 3200 assumes FFT_ENC == 512");
_Static_assert(FFT_DEC == FFT_ENC, "Encoder/decoder FFT sizes must match");
_Static_assert(PE_FFT_SIZE == 512, "Pitch estimator FFT size must be 512");
_Static_assert((M_PITCH % DEC) == 0, "M_PITCH must be divisible by DEC");
_Static_assert(NDEC == (M_PITCH / DEC), "NDEC must equal M_PITCH / DEC");
_Static_assert(NLP_NTAP == 48, "NLP FIR unrolling assumes NLP_NTAP == 48");
_Static_assert(WO_BITS <= 8, "encode_Wo() assumes <= 8-bit fields");
_Static_assert(E_BITS <= 8, "encode_energy() assumes <= 8-bit fields");
_Static_assert(TW <= N_SAMP / 2, "Parzen window ramp too long");

/* consts */
extern const float nlp_fir[NLP_NTAP];
extern const float bw_gamma[LPC_ORD + 1];
extern const uint16_t delta_lsp_cb[LPC_ORD][32];

typedef kiss_fft_cpx complex_t;

typedef struct model_t
{
    float Wo;
    int L;
    float A[MAX_AMP + 1];
    float phi[MAX_AMP + 1];
    int voiced;
} model_t;

typedef struct nlp_t
{
    float w[PMAX_M / DEC] CODEC2_ALIGNED;
    float sq[PMAX_M] CODEC2_ALIGNED;
    float sq_fir[NDEC] CODEC2_ALIGNED;
    float mem_x, mem_y;
    kiss_fftr_cfg fftr_cfg;
    float fftr_buff[PE_FFT_SIZE] CODEC2_ALIGNED;
    complex_t Fw[PE_FFT_SIZE / 2 + 1] CODEC2_ALIGNED;
#ifdef CODEC2_ESP32S3_DSP
    /* Interleaved complex buffer for esp-dsp FFT [Re0,Im0,Re1,Im1,...] */
    float fft_interleaved[PE_FFT_SIZE * 2] CODEC2_ALIGNED;
#endif
} nlp_t;

typedef struct codec2_t
{
    uint32_t next_rn;

    /* Analysis window and spectrum - aligned for SIMD */
    float w[M_PITCH] CODEC2_ALIGNED;
    float W[FFT_ENC] CODEC2_ALIGNED;
    float Pn[2 * N_SAMP] CODEC2_ALIGNED;
    float Sn[M_PITCH] CODEC2_ALIGNED;
    nlp_t nlp;

    /* Synthesis buffers - aligned for SIMD */
    float Sn_[2 * N_SAMP] CODEC2_ALIGNED;
    float ex_phase;
    float bg_est;
    float prev_f0_enc;

    model_t prev_model_dec;
    float prev_lsps_dec[LPC_ORD] CODEC2_ALIGNED;
    float prev_e_dec;

    /* kiss_fft configurations (used when esp-dsp not available) */
    kiss_fft_cfg fft_fwd_cfg;
    kiss_fftr_cfg fftr_fwd_cfg;
    kiss_fftr_cfg fftr_inv_cfg;
    kiss_fft_cfg phase_fft_fwd_cfg;
    kiss_fft_cfg phase_fft_inv_cfg;

    /* FFT scratch buffer - aligned for SIMD operations */
    kiss_fft_cpx fft_buffer[FFT_ENC] CODEC2_ALIGNED;

#ifdef CODEC2_ESP32S3_DSP
    /*
     * esp-dsp interleaved complex buffer for FFT operations
     * Format: [Re0, Im0, Re1, Im1, ...]
     * Size: FFT_ENC * 2 floats
     * NOTE: When codec2_t is statically allocated, ensure it's in internal SRAM
     * for optimal PIE performance (use CODEC2_SRAM_ATTR on the instance)
     */
    float fft_espdsp[FFT_ENC * 2] CODEC2_ALIGNED;

    /* LPC autocorrelation buffer - aligned for dsps_dotprod_f32 */
    float lpc_R[LPC_ORD + 1] CODEC2_ALIGNED;

    /* Windowed speech buffer for LPC analysis - aligned for SIMD */
    float lpc_Wn[M_PITCH] CODEC2_ALIGNED;
#endif

    /*
     * fft_buffer scratch usage:
     * - only one logical use at a time
     * - no overlapping lifetimes
     * - sizes are semantic (FFT_ENC, FFT_DEC)
     * Violating this will cause silent DSP corruption.
     */
    uint8_t fft_fwd_mem[FFT_FWD_MEM_BYTES] CODEC2_ALIGNED;
    uint8_t fftr_fwd_mem[FFTR_MEM_BYTES] CODEC2_ALIGNED;
    uint8_t fftr_inv_mem[FFTR_MEM_BYTES] CODEC2_ALIGNED;
} codec2_t;

_Static_assert(sizeof(((codec2_t *)0)->fft_buffer) >= FFT_ENC * sizeof(kiss_fft_cpx), "fft_buffer too small for FFT_ENC scratch");

/*
 * ESP32-S3 specific compile-time checks
 * Ensure data structures are compatible with esp-dsp SIMD operations
 */
#ifdef CODEC2_ESP32S3_DSP

/* Verify kiss_fft_cpx is compatible with interleaved float format
 * Required for safe casting between formats */
_Static_assert(sizeof(kiss_fft_cpx) == 2 * sizeof(float),
    "kiss_fft_cpx must be 8 bytes for esp-dsp interleaved format compatibility");

/* Verify esp-dsp FFT buffer is large enough */
_Static_assert(sizeof(((codec2_t *)0)->fft_espdsp) >= FFT_ENC * 2 * sizeof(float),
    "fft_espdsp must hold FFT_ENC interleaved complex values");

/* Verify LPC autocorrelation buffer is correctly sized */
_Static_assert(sizeof(((codec2_t *)0)->lpc_R) >= (LPC_ORD + 1) * sizeof(float),
    "lpc_R must hold LPC_ORD+1 autocorrelation coefficients");

/* Verify NLP interleaved FFT buffer is correctly sized */
_Static_assert(sizeof(((nlp_t *)0)->fft_interleaved) >= PE_FFT_SIZE * 2 * sizeof(float),
    "nlp fft_interleaved must hold PE_FFT_SIZE interleaved complex values");

#endif /* CODEC2_ESP32S3_DSP */

#endif /* CODEC2_MOD_INTERNAL_H */
