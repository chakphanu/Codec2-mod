#include "codec2_mod.h"
#include "codec2_internal.h"
#include "analysis.h"
#include "synthesis.h"
#include "nlp.h"
#include "lpc.h"
#include "interp.h"
#include "quantise.h"

void codec2_init(codec2_t *c2)
{
	codec2_encoder_init(&c2->encoder);
	codec2_decoder_init(&c2->decoder);
}

void codec2_encoder_init(codec2_encoder_t *c2)
{
	for (int i = 0; i < M_PITCH; i++)
		c2->Sn[i] = 1.0f;

	size_t mem;

	mem = sizeof(c2->fftr_fwd_mem);
	c2->fftr_fwd_cfg = kiss_fftr_alloc(FFT_ENC, 0, c2->fftr_fwd_mem, &mem);

	c2->nlp.fftr_cfg = c2->fftr_fwd_cfg;

	analysis_init(c2);

	c2->prev_f0_enc = 1.0f / P_MAX_S;

	nlp_init(&c2->nlp);
}

void codec2_decoder_init(codec2_decoder_t *c2)
{
	c2->next_rn = 1;

	memset(c2->Sn_, 0, sizeof(c2->Sn_));

	size_t mem;

	mem = sizeof(c2->fftr_fwd_mem);
	c2->fftr_fwd_cfg = kiss_fftr_alloc(FFT_ENC, 0, c2->fftr_fwd_mem, &mem);

	mem = sizeof(c2->fftr_inv_mem);
	c2->fftr_inv_cfg = kiss_fftr_alloc(FFT_DEC, 1, c2->fftr_inv_mem, &mem);

	synthesis_init(c2);

	c2->bg_est = 0.0;
	c2->ex_phase = 0.0;

	codec2_params_init(&c2->params);
}

void codec2_pack_params(uint8_t bits[CODEC2_BYTES_PER_FRAME], int v0, const codec2_half_params *p1)
{
	int lspd_indexes[LPC_ORD];
	unsigned int nbit = 0;

	memset(bits, 0, BYTES_PER_FRAME); // 64 bits

	pack(bits, &nbit, v0 != 0, 1);
	pack(bits, &nbit, p1->voiced != 0, 1);
	pack(bits, &nbit, encode_Wo(p1->Wo, WO_BITS), WO_BITS);
	pack(bits, &nbit, encode_energy(p1->e, E_BITS), E_BITS);

	encode_lspds_scalar(lspd_indexes, p1->lsp);
	for (int i = 0; i < LSPD_SCALAR_INDEXES; i++)
	{
		pack(bits, &nbit, lspd_indexes[i], 5); // codebook indices are 5 bits
	}
}

void codec2_encode(codec2_encoder_t *c2, uint8_t *bits, const int16_t *speech)
{
	model_t model = {0};
	float ak[LPC_ORD + 1];
	codec2_half_params p1;
	int v0;

	/* first 10ms analysis frame - we just want voicing */
	analyse_one_frame(c2, &model, speech);
	v0 = model.voiced;

	/* second 10ms analysis frame */
	analyse_one_frame(c2, &model, &speech[N_SAMP]);
	p1.Wo = model.Wo;
	p1.L = model.L;
	p1.voiced = model.voiced;

	speech_to_uq_lsps(c2, p1.lsp, ak, &p1.e, c2->Sn, c2->w);

	codec2_pack_params(bits, v0, &p1);
}

void codec2_wo_limits(float *wo_min, float *wo_max)
{
	if (wo_min)
		*wo_min = W0_MIN;
	if (wo_max)
		*wo_max = W0_MAX;
}

void codec2_model_opts_default(codec2_model_opts *o)
{
	o->post_filter = 1;
	o->bass_boost = 1;
	o->beta = LPCPF_BETA;
	o->gamma = LPCPF_GAMMA;
	o->lpc_correction = 1;
}

void codec2_params_init(codec2_params_state *st)
{
	st->prev_Wo = TWO_PI / P_MAX;
	st->prev_L = M_PI / st->prev_Wo;
	st->prev_voiced = 0;
	st->prev_e = 1;

	for (int i = 0; i < LPC_ORD; i++)
		st->prev_lsps[i] = i * M_PI / (LPC_ORD + 1);
}

void codec2_decode_params(codec2_params_state *st, const uint8_t bits[CODEC2_BYTES_PER_FRAME], codec2_half_params p[2])
{
	model_t model[2] = {0};
	model_t prev = {0};
	int lspd_indexes[LPC_ORD];
	int Wo_index, e_index;

	/* only Wo and voicing of the previous half are used by interp_Wo() */
	prev.Wo = st->prev_Wo;
	prev.L = st->prev_L;
	prev.voiced = st->prev_voiced;

	/* unpack bits from channel ------------------------------------*/
	unsigned int nbit = 0;
	/* this will partially fill the model params for the 2 x 10ms
	   frames */
	model[0].voiced = unpack(bits, &nbit, 1);
	model[1].voiced = unpack(bits, &nbit, 1);

	Wo_index = unpack(bits, &nbit, WO_BITS);
	model[1].Wo = decode_Wo(Wo_index, WO_BITS);
	model[1].L = M_PI / model[1].Wo;

	e_index = unpack(bits, &nbit, E_BITS);
	p[1].e = decode_energy(e_index, E_BITS);

	for (int i = 0; i < LSPD_SCALAR_INDEXES; i++)
	{
		lspd_indexes[i] = unpack(bits, &nbit, 5); // codebook indices are 5 bits
	}
	decode_lspds_scalar(p[1].lsp, lspd_indexes);

	/* interpolate ------------------------------------------------*/
	/* Wo and energy are sampled every 20ms, so we interpolate just 1
	   10ms frame between 20ms samples */
	interp_Wo(&model[0], &prev, &model[1]);
	p[0].e = interp_energy(st->prev_e, p[1].e);

	/* LSPs are sampled every 20ms so we interpolate the frame in
	   between */
	interpolate_lsp(p[0].lsp, st->prev_lsps, p[1].lsp);

	for (int i = 0; i < 2; i++)
	{
		p[i].Wo = model[i].Wo;
		p[i].L = model[i].L;
		p[i].voiced = model[i].voiced;
	}

	/* update memories for next frame ----------------------------*/
	st->prev_Wo = p[1].Wo;
	st->prev_L = p[1].L;
	st->prev_voiced = p[1].voiced;
	st->prev_e = p[1].e;
	for (int i = 0; i < LPC_ORD; i++)
		st->prev_lsps[i] = p[1].lsp[i];
}

void codec2_params_to_model(codec2_decoder_t *c2, const codec2_half_params p[2], const codec2_model_opts *opts,
							codec2_model_t m[2], codec2_complex_t Aw[2][CODEC2_AW_BINS])
{
	codec2_model_opts defaults;
	float ak[LPC_ORD + 1];
	/* A2 scratch lives in the top quarter of fft_buffer: aks_to_mag2_opts()
	   only uses the first FFT_ENC / 2 + 1 complex values of it (the a[] /
	   ag[] inputs and the A_gamma spectrum), so this keeps the stack small
	   now that each half has its own Aw[] */
	float *A2 = (float *)c2->fft_buffer + 2 * FFT_DEC - FFT_ENC / 2;

	if (!opts)
	{
		codec2_model_opts_default(&defaults);
		opts = &defaults;
	}

	/* Each half gets its own Aw[], so both halves can be modelled before
	   synthesis. aks_to_mag2_opts() only uses fft_buffer and the forward
	   FFT tmpbuf as scratch (both fully written before being read), and
	   synthesis never reads them, so this order gives the same bytes as
	   the interleaved model/synthesise order of the original decoder. */
	for (int i = 0; i < 2; i++)
	{
		memset(&m[i], 0, sizeof(m[i]));
		m[i].Wo = p[i].Wo;
		m[i].L = p[i].L;
		m[i].voiced = p[i].voiced;

		lsp_to_lpc(p[i].lsp, ak);
		aks_to_mag2_opts(c2, ak, &m[i], p[i].e, opts->post_filter, opts->bass_boost,
						 opts->beta, opts->gamma, Aw[i], A2);
		if (opts->lpc_correction)
			apply_lpc_correction(&m[i]);
	}
}

void codec2_synthesise(codec2_decoder_t *c2, const codec2_model_t m[2], const codec2_complex_t Aw[2][CODEC2_AW_BINS],
					   int16_t speech[CODEC2_SAMPLES_PER_FRAME])
{
	for (int i = 0; i < 2; i++)
		synthesise_one_frame(c2, &speech[N_SAMP * i], &m[i], Aw[i], 1.0f);
}

void codec2_decode(codec2_decoder_t *c2, int16_t *speech, const uint8_t *bits)
{
	codec2_half_params p[2];
	model_t m[2];
	complex_t Aw[2][CODEC2_AW_BINS];

	codec2_decode_params(&c2->params, bits, p);
	codec2_params_to_model(c2, p, NULL, m, Aw);
	codec2_synthesise(c2, m, (const complex_t(*)[CODEC2_AW_BINS])Aw, speech);
}

size_t codec2_size(void)
{
	return sizeof(codec2_t);
}

size_t codec2_encoder_size(void)
{
	return sizeof(codec2_encoder_t);
}

size_t codec2_decoder_size(void)
{
	return sizeof(codec2_decoder_t);
}

size_t codec2_state_alignment(void)
{
	/* codec2_t contains both structs, so its alignment covers each of them */
	return _Alignof(codec2_t);
}

void codec2_decoder_reseed(codec2_decoder_t *c2, uint32_t seed)
{
	c2->next_rn = seed;
}

const char *codec2_mod_version(void)
{
	return "codec2-mod 3200 vocoder-api 2";
}
