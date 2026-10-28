#ifndef CODEC2_MOD_TYPES_H
#define CODEC2_MOD_TYPES_H

/*
  Types and sizes shared by the public header (codec2_mod.h) and the
  internal headers (codec2_internal.h, kiss_fft.h), so each type has
  exactly one definition. Only CODEC2_* macros and codec2_* types live
  here; this header must stay C99 clean and include nothing but stdint.h.
*/

#include <stdint.h>

/* frame layout of the 3200 bps mode */
#define CODEC2_BYTES_PER_FRAME 8
#define CODEC2_SAMPLES_PER_FRAME 160
#define CODEC2_SAMPLES_PER_HALF 80 /* one 10 ms model frame */

/* model and spectrum sizes */
#define CODEC2_LPC_ORDER 10
#define CODEC2_MAX_AMP 80
#define CODEC2_FFT_SIZE 512
#define CODEC2_AW_BINS (CODEC2_FFT_SIZE / 2 + 1) /* real FFT bins 0..FFT/2 */

/* range of the 7-bit Wo quantiser in rad/sample, 2 pi / 160 .. 2 pi / 20
   (50 .. 400 Hz at 8 kHz); the same values as W0_MIN / W0_MAX */
#define CODEC2_WO_MIN (2.0 * 3.14159265358979323846 / 160.0)
#define CODEC2_WO_MAX (2.0 * 3.14159265358979323846 / 20.0)

/* complex value, float real and imaginary parts (the internal complex_t) */
typedef struct codec2_complex_t
{
    float r;
    float i;
} codec2_complex_t;

/* sinusoidal model of one 10 ms half frame (the internal model_t) */
typedef struct codec2_model_t
{
    float Wo;                    /* fundamental, rad/sample */
    int L;                       /* number of harmonics */
    float A[CODEC2_MAX_AMP + 1]; /* harmonic amplitudes A[1..L], A[0] unused */
    int voiced;                  /* 1 = voiced */
} codec2_model_t;

/* decoded Codec2 parameters of one 10 ms half frame */
typedef struct
{
    float Wo;                      /* fundamental, rad/sample */
    int L;                         /* number of harmonics, (int)(pi / Wo) */
    int voiced;                    /* 1 = voiced */
    float e;                       /* linear LPC energy */
    float lsp[CODEC2_LPC_ORDER];   /* LSPs in radians */
} codec2_half_params;

/* parameter decoder history: the second half of the previous frame.
   Same fields as c2f_params_state in Codec2-mod-neural. */
typedef struct
{
    float prev_Wo;
    int prev_L;
    int prev_voiced;
    float prev_e;
    float prev_lsps[CODEC2_LPC_ORDER];
} codec2_params_state;

#endif /* CODEC2_MOD_TYPES_H */
