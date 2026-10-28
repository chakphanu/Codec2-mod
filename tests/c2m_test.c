/*
  c2m_test - checks of the Codec2-mod vocoder API on one raw PCM file.

  usage: c2m_test input.raw reference.bit [reference_decoded.raw]

  input.raw              8 kHz s16le mono speech
  reference.bit          expected codec2_encode() output (8 bytes per frame),
                         e.g. from upstream "c2enc 3200"
  reference_decoded.raw  optional expected codec2_decode() output of
                         reference.bit (e.g. from the main branch)

  (a) codec2_encode() equals reference.bit
  (b) codec2_decode() equals codec2_decode_params() +
      codec2_params_to_model(NULL) + codec2_synthesise()
      (and reference_decoded.raw when given)
  (c) codec2_params_to_model(defaults) equals codec2_params_to_model(NULL)
  (d) determinism: two fresh decoders, and a decoder reseeded to 12345 and
      back to 1, give identical PCM
  (e) codec2_pack_params() with the encoder's analysed parameters, and the
      public LPC helper chain, reproduce codec2_encode()
  (f) codec2_decode_params() equals the decoder's own parameters (its
      history after codec2_decode()) and the parameter decoding of main's
      codec2_decode() written out with the exported internals (the way
      Codec2-mod-neural's c2f_frame_features() does it)
  (g) CODEC2_WO_MIN / CODEC2_WO_MAX and codec2_wo_limits() equal the
      quantiser limits W0_MIN / W0_MAX

  Uses the default include of codec2_mod.h (internals visible).
  Exit status 0 when all checks pass.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "codec2_mod.h"
#include "analysis.h"
#include "interp.h"
#include "lpc.h"
#include "quantise.h"

#define FRAME_BYTES CODEC2_BYTES_PER_FRAME
#define FRAME_SAMPLES CODEC2_SAMPLES_PER_FRAME

#define CONST_AW(aw) ((const codec2_complex_t(*)[CODEC2_AW_BINS])(aw))

static int failures;

static void report(const char *name, int ok, const char *detail)
{
    printf("%-4s %s%s%s\n", ok ? "PASS" : "FAIL", name, detail[0] ? ": " : "", detail);
    if (!ok)
        failures++;
}

static void *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc(n > 0 ? (size_t)n : 1);
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n)
    {
        fprintf(stderr, "cannot read %s\n", path);
        exit(2);
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static size_t count_frame_diffs(const void *a, const void *b, size_t frames, size_t frame_size, long *first)
{
    const uint8_t *pa = a, *pb = b;
    size_t diffs = 0;
    *first = -1;
    for (size_t i = 0; i < frames; i++)
    {
        if (memcmp(pa + i * frame_size, pb + i * frame_size, frame_size))
        {
            if (*first < 0)
                *first = (long)i;
            diffs++;
        }
    }
    return diffs;
}

static void report_frames(const char *name, size_t frames, size_t diffs, long first)
{
    char detail[128];
    snprintf(detail, sizeof(detail), "%zu/%zu frames identical (first diff %ld)", frames - diffs, frames, first);
    report(name, diffs == 0, detail);
}

/* The parameter half of main's codec2_decode(), written out with the
   exported internals (what Codec2-mod-neural's c2f_frame_features() does).
   prev_model / prev_e / prev_lsps are main's decoder history fields. */
typedef struct
{
    model_t prev_model;
    float prev_e;
    float prev_lsps[LPC_ORD];
} main_history_t;

static void main_history_init(main_history_t *h)
{
    memset(h, 0, sizeof(*h));
    h->prev_model.Wo = TWO_PI / P_MAX;
    h->prev_model.L = M_PI / h->prev_model.Wo;
    h->prev_model.voiced = 0;
    for (int i = 0; i < LPC_ORD; i++)
        h->prev_lsps[i] = i * M_PI / (LPC_ORD + 1);
    h->prev_e = 1;
}

static void main_decode_params(main_history_t *h, const uint8_t *bits, codec2_half_params p[2])
{
    model_t model[2] = {{0}};
    int lspd_indexes[LPC_ORD];
    float lsps[2][LPC_ORD];
    float e[2];
    unsigned int nbit = 0;

    model[0].voiced = unpack(bits, &nbit, 1);
    model[1].voiced = unpack(bits, &nbit, 1);
    model[1].Wo = decode_Wo(unpack(bits, &nbit, WO_BITS), WO_BITS);
    model[1].L = M_PI / model[1].Wo;
    e[1] = decode_energy(unpack(bits, &nbit, E_BITS), E_BITS);
    for (int i = 0; i < LSPD_SCALAR_INDEXES; i++)
        lspd_indexes[i] = unpack(bits, &nbit, 5);
    decode_lspds_scalar(&lsps[1][0], lspd_indexes);

    interp_Wo(&model[0], &h->prev_model, &model[1]);
    e[0] = interp_energy(h->prev_e, e[1]);
    interpolate_lsp(&lsps[0][0], h->prev_lsps, &lsps[1][0]);

    for (int i = 0; i < 2; i++)
    {
        p[i].Wo = model[i].Wo;
        p[i].L = model[i].L;
        p[i].voiced = model[i].voiced;
        p[i].e = e[i];
        memcpy(p[i].lsp, lsps[i], sizeof(p[i].lsp));
    }

    h->prev_model = model[1];
    h->prev_e = e[1];
    memcpy(h->prev_lsps, lsps[1], sizeof(h->prev_lsps));
}

/* state is large: keep it out of the stack */
static codec2_encoder_t enc_a, enc_b;
static codec2_decoder_t dec_a, dec_b, dec_c, dec_d, dec_e;

typedef struct
{
    codec2_half_params p[2];
    codec2_model_t m[2];
    codec2_complex_t Aw[2][CODEC2_AW_BINS];
} frame_t;
static frame_t frame_a, frame_b;

int main(int argc, char **argv)
{
    char detail[256];

    if (argc < 3 || argc > 4)
    {
        fprintf(stderr, "usage: %s input.raw reference.bit [reference_decoded.raw]\n", argv[0]);
        return 2;
    }

    size_t pcm_len, ref_len, refdec_len = 0;
    int16_t *pcm = read_file(argv[1], &pcm_len);
    uint8_t *ref = read_file(argv[2], &ref_len);
    int16_t *refdec = argc == 4 ? read_file(argv[3], &refdec_len) : NULL;

    size_t frames = pcm_len / (FRAME_SAMPLES * sizeof(int16_t));
    size_t ref_frames = ref_len / FRAME_BYTES;

    printf("%s, %s\n", codec2_mod_version(), argv[1]);
    printf("frames: input %zu, reference %zu\n", frames, ref_frames);

    uint8_t *bits = calloc(frames + 1, FRAME_BYTES);
    uint8_t *bits_e = calloc(frames + 1, FRAME_BYTES);
    uint8_t *bits_lpc = calloc(frames + 1, FRAME_BYTES);
    size_t dframes = ref_frames;
    size_t dsamples = dframes * FRAME_SAMPLES;
    int16_t *out_a = calloc(dsamples + 1, sizeof(int16_t));
    int16_t *out_b = calloc(dsamples + 1, sizeof(int16_t));
    int16_t *out_c = calloc(dsamples + 1, sizeof(int16_t));
    int16_t *out_d = calloc(dsamples + 1, sizeof(int16_t));
    int16_t *out_e = calloc(dsamples + 1, sizeof(int16_t));
    if (!bits || !bits_e || !bits_lpc || !out_a || !out_b || !out_c || !out_d || !out_e)
        return 2;

    /* (a) + (e): encode three ways ------------------------------------- */
    codec2_encoder_init(&enc_a);
    codec2_encoder_init(&enc_b);
    size_t lpc_fallback = 0;
    for (size_t f = 0; f < frames; f++)
    {
        const int16_t *s = &pcm[f * FRAME_SAMPLES];
        codec2_encode(&enc_a, &bits[f * FRAME_BYTES], s);

        /* the analysis half of codec2_encode() with the internals, then
           codec2_pack_params() */
        model_t model = {0};
        float ak_enc[LPC_ORD + 1];
        codec2_half_params p1;
        int v0;

        analyse_one_frame(&enc_b, &model, s);
        v0 = model.voiced;
        analyse_one_frame(&enc_b, &model, &s[N_SAMP]);
        p1.Wo = model.Wo;
        p1.L = model.L;
        p1.voiced = model.voiced;
        speech_to_uq_lsps(&enc_b, p1.lsp, ak_enc, &p1.e, enc_b.Sn, enc_b.w);
        codec2_pack_params(&bits_e[f * FRAME_BYTES], v0, &p1);

        /* public LPC helper chain on the encoder's windowed speech:
           autocorrelation over the window span as in lpc.c */
        {
            const int w_start = M_PITCH / 2 - NW / 2;
            const int w_end = w_start + NW;
            float wn[M_PITCH];
            float R[CODEC2_LPC_ORDER + 1], ak[CODEC2_LPC_ORDER + 1];
            codec2_half_params q1 = p1;
            float e = 0.0f;

            memset(wn, 0, sizeof(wn));
            for (int i = w_start; i < w_end; i++)
            {
                wn[i] = enc_b.Sn[i] * enc_b.w[i];
                e += wn[i] * wn[i];
            }
            if (e == 0.0f)
            {
                /* encoder fallback: flat LSPs, zero energy (kept from p1) */
                lpc_fallback++;
            }
            else
            {
                for (int j = 0; j <= CODEC2_LPC_ORDER; j++)
                {
                    float acc = 0.0f;
                    for (int i = w_start; i < w_end - j; i++)
                        acc += wn[i] * wn[i + j];
                    R[j] = acc;
                }
                codec2_levinson_durbin(R, ak);
                e = 0.0f;
                for (int i = 0; i <= CODEC2_LPC_ORDER; i++)
                    e += ak[i] * R[i];
                codec2_lpc_bw_expand(ak);
                codec2_lpc_to_lsp(ak, q1.lsp);
                q1.e = e;
            }
            codec2_pack_params(&bits_lpc[f * FRAME_BYTES], v0, &q1);
        }
    }

    {
        long first;
        size_t n = frames < ref_frames ? frames : ref_frames;
        size_t d = count_frame_diffs(bits, ref, n, FRAME_BYTES, &first);
        snprintf(detail, sizeof(detail), "%zu/%zu frames identical%s (first diff %ld)",
                 n - d, n, frames == ref_frames ? "" : ", frame count differs", first);
        report("(a) codec2_encode == reference.bit", d == 0 && frames == ref_frames, detail);

        d = count_frame_diffs(bits, bits_e, frames, FRAME_BYTES, &first);
        report_frames("(e) analysis + codec2_pack_params == codec2_encode", frames, d, first);

        d = count_frame_diffs(bits, bits_lpc, frames, FRAME_BYTES, &first);
        snprintf(detail, sizeof(detail), "%zu/%zu frames identical (first diff %ld, %zu zero-energy frames)",
                 frames - d, frames, first, lpc_fallback);
        report("(e) levinson_durbin + lpc_bw_expand + lpc_to_lsp + pack_params == codec2_encode", d == 0, detail);
    }

    /* (b) (c) (d) (f): decode reference.bit several ways ----------------- */
    codec2_decoder_init(&dec_a); /* codec2_decode */
    codec2_decoder_init(&dec_b); /* decode_params + params_to_model(NULL) + synthesise */
    codec2_decoder_init(&dec_c); /* decode_params + params_to_model(defaults) + synthesise */
    codec2_decoder_init(&dec_d); /* second fresh codec2_decode */
    codec2_decoder_init(&dec_e); /* reseeded to 12345 then back to 1 */
    codec2_decoder_reseed(&dec_e, 12345u);
    codec2_decoder_reseed(&dec_e, 1u);

    codec2_model_opts opts;
    codec2_model_opts_default(&opts);

    codec2_params_state own;   /* codec2_decode_params on its own */
    main_history_t mainh;      /* main's parameter decoding, written out */
    codec2_params_init(&own);
    main_history_init(&mainh);

    size_t init_same = memcmp(&own, &dec_a.params, sizeof(own)) == 0;
    size_t model_diffs = 0, hist_diffs = 0, main_diffs = 0;
    long model_first = -1, hist_first = -1, main_first = -1;
    for (size_t f = 0; f < dframes; f++)
    {
        const uint8_t *b = &ref[f * FRAME_BYTES];
        size_t o = f * FRAME_SAMPLES;
        codec2_half_params p_own[2], p_main[2];

        codec2_decode(&dec_a, &out_a[o], b);

        codec2_decode_params(&dec_b.params, b, frame_a.p);
        codec2_params_to_model(&dec_b, frame_a.p, NULL, frame_a.m, frame_a.Aw);
        codec2_synthesise(&dec_b, frame_a.m, CONST_AW(frame_a.Aw), &out_b[o]);

        codec2_decode_params(&dec_c.params, b, frame_b.p);
        codec2_params_to_model(&dec_c, frame_b.p, &opts, frame_b.m, frame_b.Aw);
        if (memcmp(&frame_a, &frame_b, sizeof(frame_a)))
        {
            if (model_first < 0)
                model_first = (long)f;
            model_diffs++;
        }
        codec2_synthesise(&dec_c, frame_b.m, CONST_AW(frame_b.Aw), &out_c[o]);

        codec2_decode(&dec_d, &out_d[o], b);
        codec2_decode(&dec_e, &out_e[o], b);

        /* (f) */
        codec2_decode_params(&own, b, p_own);
        if (memcmp(&own, &dec_a.params, sizeof(own)) || memcmp(p_own, frame_a.p, sizeof(p_own)))
        {
            if (hist_first < 0)
                hist_first = (long)f;
            hist_diffs++;
        }
        main_decode_params(&mainh, b, p_main);
        if (memcmp(p_own, p_main, sizeof(p_own)))
        {
            if (main_first < 0)
                main_first = (long)f;
            main_diffs++;
        }
    }

    {
        long first;
        size_t d = count_frame_diffs(out_a, out_b, dframes, FRAME_SAMPLES * 2, &first);
        report_frames("(b) codec2_decode == decode_params + params_to_model(NULL) + synthesise", dframes, d, first);

        if (refdec)
        {
            size_t nbytes = dsamples * sizeof(int16_t);
            int same = refdec_len == nbytes && !memcmp(out_a, refdec, nbytes);
            snprintf(detail, sizeof(detail), "%zu vs %zu bytes", nbytes, refdec_len);
            report("(b) codec2_decode == reference_decoded.raw", same, detail);
        }

        report_frames("(c) params_to_model(defaults) == params_to_model(NULL) (params, model, Aw)", dframes,
                      model_diffs, model_first);

        d = count_frame_diffs(out_a, out_c, dframes, FRAME_SAMPLES * 2, &first);
        report_frames("(c) PCM via params_to_model(defaults) == codec2_decode", dframes, d, first);

        d = count_frame_diffs(out_a, out_d, dframes, FRAME_SAMPLES * 2, &first);
        report_frames("(d) two fresh decoders give identical PCM", dframes, d, first);

        d = count_frame_diffs(out_a, out_e, dframes, FRAME_SAMPLES * 2, &first);
        report_frames("(d) decoder after codec2_decoder_reseed(.., 1) gives identical PCM", dframes, d, first);

        report("(f) codec2_params_init == codec2_decoder_init's history", (int)init_same, "");
        report_frames("(f) codec2_decode_params == decoder history after codec2_decode", dframes, hist_diffs,
                      hist_first);
        report_frames("(f) codec2_decode_params == main's parameter decoding (exported internals)", dframes,
                      main_diffs, main_first);
    }

    /* (g) Wo limits */
    {
        float lo, hi;
        codec2_wo_limits(&lo, &hi);
        int ok = CODEC2_WO_MIN == W0_MIN && CODEC2_WO_MAX == W0_MAX &&
                 lo == (float)W0_MIN && hi == (float)W0_MAX;
        snprintf(detail, sizeof(detail), "%.9g .. %.9g rad/sample", lo, hi);
        report("(g) CODEC2_WO_MIN/MAX == codec2_wo_limits() == W0_MIN/MAX", ok, detail);
    }

    /* info: the knobs are wired (expected to differ) */
    {
        codec2_decoder_init(&dec_d);
        codec2_decoder_reseed(&dec_d, 12345u);
        codec2_decoder_init(&dec_e);
        codec2_model_opts no_pf;
        codec2_model_opts_default(&no_pf);
        no_pf.post_filter = 0;
        size_t seed_diff = 0, pf_diff = 0;
        for (size_t f = 0; f < dframes; f++)
        {
            const uint8_t *b = &ref[f * FRAME_BYTES];
            int16_t tmp[FRAME_SAMPLES];
            codec2_decode(&dec_d, tmp, b);
            seed_diff += memcmp(tmp, &out_a[f * FRAME_SAMPLES], sizeof(tmp)) != 0;
            codec2_decode_params(&dec_e.params, b, frame_b.p);
            codec2_params_to_model(&dec_e, frame_b.p, &no_pf, frame_b.m, frame_b.Aw);
            codec2_synthesise(&dec_e, frame_b.m, CONST_AW(frame_b.Aw), tmp);
            pf_diff += memcmp(tmp, &out_a[f * FRAME_SAMPLES], sizeof(tmp)) != 0;
        }
        printf("info frames that change with seed 12345: %zu/%zu, with post_filter=0: %zu/%zu\n",
               seed_diff, dframes, pf_diff, dframes);
    }

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");

    free(pcm);
    free(ref);
    free(refdec);
    free(bits);
    free(bits_e);
    free(bits_lpc);
    free(out_a);
    free(out_b);
    free(out_c);
    free(out_d);
    free(out_e);
    return failures ? 1 : 0;
}
