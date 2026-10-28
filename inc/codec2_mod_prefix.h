/*
  Optional symbol prefix (off by default).

  Build the library AND compile its callers with -DCODEC2_MOD_PREFIX=<p>
  (for example -DCODEC2_MOD_PREFIX=c2m_) to give every global symbol of
  the library the prefix <p>: the codec2_* API (codec2_encode becomes
  c2m_codec2_encode) and the exported internals (pack becomes c2m_pack,
  kiss_fft becomes c2m_kiss_fft, ...). Source code keeps using the plain
  names; the macros below rename them. -DCODEC2_MOD_PREFIX without a
  value uses c2m_.

  This is for a program that must link upstream libcodec2 into the same
  binary (both define codec2_encode, codec2_decode, pack, kiss_fft...).
  Keep code that includes upstream codec2.h and code that includes this
  library's headers in separate translation units.

  The function names are renamed with function-like macros, so a name is
  only replaced where it is followed by "(" (struct members and
  parameters with the same name, e.g. nlp, are left alone). With
  CODEC2_MOD_PUBLIC_ONLY only the codec2_* API names are defined here.

  This header has no include guard of its own: each of its two sections
  is guarded separately.
*/

#ifdef CODEC2_MOD_PREFIX

#ifndef CODEC2_MOD_PREFIX_DEFS_
#define CODEC2_MOD_PREFIX_DEFS_
#if (CODEC2_MOD_PREFIX + 0) == 1
#define CODEC2_MOD_PREFIX_TOKEN_ c2m_
#else
#define CODEC2_MOD_PREFIX_TOKEN_ CODEC2_MOD_PREFIX
#endif
#define CODEC2_MOD_CAT_(a, b) a##b
#define CODEC2_MOD_XCAT_(a, b) CODEC2_MOD_CAT_(a, b)
#define CODEC2_MOD_PFX_(name) CODEC2_MOD_XCAT_(CODEC2_MOD_PREFIX_TOKEN_, name)

/* public API */
#define codec2_init(...) CODEC2_MOD_PFX_(codec2_init)(__VA_ARGS__)
#define codec2_encoder_init(...) CODEC2_MOD_PFX_(codec2_encoder_init)(__VA_ARGS__)
#define codec2_decoder_init(...) CODEC2_MOD_PFX_(codec2_decoder_init)(__VA_ARGS__)
#define codec2_encode(...) CODEC2_MOD_PFX_(codec2_encode)(__VA_ARGS__)
#define codec2_decode(...) CODEC2_MOD_PFX_(codec2_decode)(__VA_ARGS__)
#define codec2_params_init(...) CODEC2_MOD_PFX_(codec2_params_init)(__VA_ARGS__)
#define codec2_decode_params(...) CODEC2_MOD_PFX_(codec2_decode_params)(__VA_ARGS__)
#define codec2_model_opts_default(...) CODEC2_MOD_PFX_(codec2_model_opts_default)(__VA_ARGS__)
#define codec2_params_to_model(...) CODEC2_MOD_PFX_(codec2_params_to_model)(__VA_ARGS__)
#define codec2_synthesise(...) CODEC2_MOD_PFX_(codec2_synthesise)(__VA_ARGS__)
#define codec2_levinson_durbin(...) CODEC2_MOD_PFX_(codec2_levinson_durbin)(__VA_ARGS__)
#define codec2_lpc_bw_expand(...) CODEC2_MOD_PFX_(codec2_lpc_bw_expand)(__VA_ARGS__)
#define codec2_lpc_to_lsp(...) CODEC2_MOD_PFX_(codec2_lpc_to_lsp)(__VA_ARGS__)
#define codec2_pack_params(...) CODEC2_MOD_PFX_(codec2_pack_params)(__VA_ARGS__)
#define codec2_wo_limits(...) CODEC2_MOD_PFX_(codec2_wo_limits)(__VA_ARGS__)
#define codec2_decoder_reseed(...) CODEC2_MOD_PFX_(codec2_decoder_reseed)(__VA_ARGS__)
#define codec2_mod_version(...) CODEC2_MOD_PFX_(codec2_mod_version)(__VA_ARGS__)
#define codec2_size(...) CODEC2_MOD_PFX_(codec2_size)(__VA_ARGS__)
#define codec2_encoder_size(...) CODEC2_MOD_PFX_(codec2_encoder_size)(__VA_ARGS__)
#define codec2_decoder_size(...) CODEC2_MOD_PFX_(codec2_decoder_size)(__VA_ARGS__)
#define codec2_state_alignment(...) CODEC2_MOD_PFX_(codec2_state_alignment)(__VA_ARGS__)
#endif /* CODEC2_MOD_PREFIX_DEFS_ */

#if !defined(CODEC2_MOD_PUBLIC_ONLY) && !defined(CODEC2_MOD_PREFIX_INTERNAL_)
#define CODEC2_MOD_PREFIX_INTERNAL_
/* exported internals: functions */
#define aks_to_mag2(...) CODEC2_MOD_PFX_(aks_to_mag2)(__VA_ARGS__)
#define aks_to_mag2_opts(...) CODEC2_MOD_PFX_(aks_to_mag2_opts)(__VA_ARGS__)
#define analyse_one_frame(...) CODEC2_MOD_PFX_(analyse_one_frame)(__VA_ARGS__)
#define analysis_init(...) CODEC2_MOD_PFX_(analysis_init)(__VA_ARGS__)
#define apply_lpc_correction(...) CODEC2_MOD_PFX_(apply_lpc_correction)(__VA_ARGS__)
#define decode_energy(...) CODEC2_MOD_PFX_(decode_energy)(__VA_ARGS__)
#define decode_lspds_scalar(...) CODEC2_MOD_PFX_(decode_lspds_scalar)(__VA_ARGS__)
#define decode_Wo(...) CODEC2_MOD_PFX_(decode_Wo)(__VA_ARGS__)
#define encode_energy(...) CODEC2_MOD_PFX_(encode_energy)(__VA_ARGS__)
#define encode_lspds_scalar(...) CODEC2_MOD_PFX_(encode_lspds_scalar)(__VA_ARGS__)
#define encode_Wo(...) CODEC2_MOD_PFX_(encode_Wo)(__VA_ARGS__)
#define fast_acosf(...) CODEC2_MOD_PFX_(fast_acosf)(__VA_ARGS__)
#define fast_atan2f(...) CODEC2_MOD_PFX_(fast_atan2f)(__VA_ARGS__)
#define fast_cosf(...) CODEC2_MOD_PFX_(fast_cosf)(__VA_ARGS__)
#define interp_energy(...) CODEC2_MOD_PFX_(interp_energy)(__VA_ARGS__)
#define interpolate_lsp(...) CODEC2_MOD_PFX_(interpolate_lsp)(__VA_ARGS__)
#define interp_Wo(...) CODEC2_MOD_PFX_(interp_Wo)(__VA_ARGS__)
#define kiss_fft(...) CODEC2_MOD_PFX_(kiss_fft)(__VA_ARGS__)
#define kiss_fft_alloc(...) CODEC2_MOD_PFX_(kiss_fft_alloc)(__VA_ARGS__)
#define kiss_fft_cleanup(...) CODEC2_MOD_PFX_(kiss_fft_cleanup)(__VA_ARGS__)
#define kiss_fft_next_fast_size(...) CODEC2_MOD_PFX_(kiss_fft_next_fast_size)(__VA_ARGS__)
#define kiss_fft_stride(...) CODEC2_MOD_PFX_(kiss_fft_stride)(__VA_ARGS__)
#define kiss_fftr(...) CODEC2_MOD_PFX_(kiss_fftr)(__VA_ARGS__)
#define kiss_fftr_alloc(...) CODEC2_MOD_PFX_(kiss_fftr_alloc)(__VA_ARGS__)
#define kiss_fftri(...) CODEC2_MOD_PFX_(kiss_fftri)(__VA_ARGS__)
#define lsp_to_lpc(...) CODEC2_MOD_PFX_(lsp_to_lpc)(__VA_ARGS__)
#define nlp(...) CODEC2_MOD_PFX_(nlp)(__VA_ARGS__)
#define nlp_init(...) CODEC2_MOD_PFX_(nlp_init)(__VA_ARGS__)
#define pack(...) CODEC2_MOD_PFX_(pack)(__VA_ARGS__)
#define speech_to_uq_lsps(...) CODEC2_MOD_PFX_(speech_to_uq_lsps)(__VA_ARGS__)
#define synthesise_one_frame(...) CODEC2_MOD_PFX_(synthesise_one_frame)(__VA_ARGS__)
#define synthesis_init(...) CODEC2_MOD_PFX_(synthesis_init)(__VA_ARGS__)
#define unpack(...) CODEC2_MOD_PFX_(unpack)(__VA_ARGS__)
/* exported internals: tables */
#define bw_gamma CODEC2_MOD_PFX_(bw_gamma)
#define delta_lsp_cb CODEC2_MOD_PFX_(delta_lsp_cb)
#define nlp_fir CODEC2_MOD_PFX_(nlp_fir)
#define nlp_fir_poly CODEC2_MOD_PFX_(nlp_fir_poly)
#endif /* internal names */

#endif /* CODEC2_MOD_PREFIX */
