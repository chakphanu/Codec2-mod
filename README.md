# Codec2-mod experimental fork
This repository contains a minimal extraction of the Codec2's 3200 bps mode, intended as a clean base for experimentation, optimization, and future research.
Only the 3200 bps mode is supported - other Codec2 modes were intentionally excluded.

The initial goal of this work was to:
- Isolate only the functions and data structures actually used by the 3200 bps mode
- Achieve bit-exact encoder output compared to the reference `libcodec2`
- Remove unused code paths, state variables, and legacy scaffolding
- Establish a codebase that is as small and optimized as possible, suitable for further development

Bit-exactness with the reference Codec2 encoder has been verified using identical input signals and byte-for-byte comparison of encoded frames.
Bit-exactness refers to the encoded bitstream - decoded audio samples may differ from the reference implementation.

Both implementations call the C library's `cosf()` during analysis, so the exact bitstream depends on its accuracy.
With an accurate `cosf()` (e.g. glibc), the encoded bitstreams are identical. With newlib-nano on a Cortex-M4,
the two differ in 1 of 150 frames of `hts1a`.

## Motivation and goals

> [!NOTE]
> Bit-exact behavior compared to the vanilla Codec2 has been achieved.
> The `main` branch contains the refactored and optimized implementation while preserving bitstream compatibility.
> 
> **The removal of floating-point math was not a goal of this work.**

Planned next steps include:
- Quantizer experiments (energy, pitch, LSPs)
- Exploration of improved excitation models
- Decoder-side enhancements that do not modify the Codec2 bitstream (eg. the use of external neural networks)
- Maintaining fitness for embedded and low-power targets

This fork is intended to be a drop-in replacement for Codec2 when using the 3200 bps mode. It is also a controlled experimental platform derived from it.
The code structure and memory usage are intentionally designed to suit embedded systems.

## Current state and implemented changes

Compared to the reference Codec2 implementation, this fork already includes:
- Complete refactoring of the 3200 bps mode into a small codebase
- Removal of unused variables, modes, code paths, and legacy state not required for 3200 bps operation
- Elimination of all persistent dynamic memory allocation (no runtime `malloc`/`free`)
- Fully deterministic, fixed-size codec state suitable for static allocation
- Much smaller flash footprint, no heap use, and a much smaller encoder stack than the reference implementation (see [Memory usage](#memory-usage))
- Verified bitstream compatibility with the reference Codec2 encoder

These changes establish a stable and minimal baseline for further optimization and experimentation.

## Speed comparison

STM32F411RE at 100 MHz, FPU enabled, *-Os* optimizations, newlib-nano, KISS FFT in all builds.
150 frames of `hts1a` (Codec2's standard test sample), timed per frame with the DWT cycle counter:

| Task                      | Reference Codec2 | Codec2-mod `main` | Codec2-mod `split-no-doubles` |
|---------------------------|------------------|-------------------|-------------------------------|
| Encoder init              | 18.07 ms¹        | 6.72 ms           | 3.08 ms                       |
| Decoder init              | 18.06 ms¹        | 5.00 ms           | 3.20 ms                       |
| Encode, avg / max         | 14.50 / 17.35 ms | 5.32 / 5.96 ms    | 4.17 / 4.55 ms                |
| Decode, avg / max         | 14.71 / 17.97 ms | 7.14 / 9.26 ms    | 5.47 / 6.61 ms                |
| Encode speedup (avg)      | 1×               | 2.7×              | 3.5×                          |
| Decode speedup (avg)      | 1×               | 2.1×              | 2.7×                          |
| CPU load, enc + dec (avg) | 146.0 %          | 62.3 %            | 48.1 %                        |

¹ The reference Codec2 has a single `codec2_create()` for both directions. It includes the heap allocation and freeing the previous instance with `codec2_destroy()`.

One frame lasts 20 ms. At 100 MHz, the reference Codec2 cannot encode and decode in real time simultaneously,
not even on average. Both Codec2-mod variants can, even in the worst-case frame (15.2 ms and 11.2 ms, respectively).

## Memory usage

Same target and build settings as above (`arm-none-eabi-gcc`, *-Os*, newlib-nano):

| Resource                          | Reference Codec2       | Codec2-mod `main` | Codec2-mod `split-no-doubles` |
|-----------------------------------|------------------------|-------------------|-------------------------------|
| Flash (code + constant data)²     | 167.2 KiB              | 23.8 KiB          | 19.5 KiB                      |
| Encoder state                     | 30.0 KiB³ (heap)       | 19.6 KiB (static) | 19.6 KiB (static)             |
| Decoder state                     | (same instance)³       | 16.2 KiB (static) | 16.2 KiB (static)             |
| Stack, encoder init               | 8.5 KiB                | 0.3 KiB           | 0.3 KiB                       |
| Stack, decoder init               | 8.5 KiB                | 0.1 KiB           | 0.1 KiB                       |
| Stack, `codec2_encode()`          | 14.1 KiB               | 1.0 KiB           | 1.0 KiB                       |
| Stack, `codec2_decode()`          | 14.4 KiB               | 5.1 KiB           | 5.1 KiB                       |
| Heap during encode/decode         | 0 B                    | 0 B               | 0 B                           |

**²** Taken from the linker map files. Includes the parts of libm, of the soft-float library and (for the reference Codec2) of the heap allocator that the codec pulls in. The reference Codec2 cannot link only the 3200 bps mode,
because `codec2_create()` references all modes.  
**³** One `codec2_create()` instance holds both the encoder and the decoder state. The figure includes the allocator's overhead.

For a full-duplex application, the total RAM (state + deepest stack) is about 44 KiB for the reference Codec2 and about 41 KiB for Codec2-mod. Codec2-mod needs no heap, and an encoder-only or decoder-only application
needs only the corresponding state.

## Branches

### `main`

The `main` branch offers an encoder that is bitstream-compatible with the reference Codec2 implementation.
The meaning, width, ordering, and allocation of all frame bit fields are all preserved. Bitstreams produced
by this encoder can be decoded by an unmodified Codec2 decoder (and vice versa).

The internal DSP implementation, floating-point operations, and decoded audio
signals are not required to be identical to the reference implementation.

### `split-no-doubles`
This branch gets rid of any double-precision arithmetic (usually by promotion).

> [!NOTE]
> Other branches may introduce experimental DSP changes (e.g. post-filters,
> quantizers, or excitation models), potentially with a modified bitstream format.

## API differences vs. reference Codec2

This fork does not use the heap-allocated `codec2_create()` / `codec2_destroy()` API from the reference Codec2.
Instead, the codec state is explicitly owned by the caller and can be allocated statically.

### Reference Codec2 (libcodec2)

```c
struct CODEC2 *c2;

c2 = codec2_create(CODEC2_MODE_3200);

codec2_encode(c2, encoded, speech);
codec2_decode(c2, speech, encoded);

codec2_destroy(c2);
```

### Codec2-mod

```c
codec2_t c2;

codec2_init(&c2);

codec2_encode(&c2.encoder, encoded, speech);
codec2_decode(&c2.decoder, speech, encoded);
```

No destroy/free function is required. The encoder and decoder can be initialized and used independently.

## Vocoder API (branch `vocoder-api`)

Additions for callers that work on the Codec2 parameters rather than only on
PCM (parameter transcoders, neural vocoder front ends). Nothing existing is
renamed: every exported function, type and macro of `main` keeps its name and
meaning, and `codec2_mod.h` still includes `codec2_internal.h` by default, so
code such as Codec2-mod-neural's `c2f_features.c` builds unchanged.
`codec2_decode()` output is byte-identical to `main`. The encoder now traps
only exactly zero frame energy like upstream (`e == 0.0f`), which makes it
bit-exact with `c2enc 3200` also on very quiet (energy index 0) frames.

The decoder is three layers; `codec2_decode(dec, speech, bits)` is exactly

```c
codec2_half_params p[2];                 /* Wo, L, voiced, e, lsp[10] per 10 ms */
codec2_model_t m[2];                     /* == model_t */
codec2_complex_t Aw[2][CODEC2_AW_BINS];  /* == complex_t, FFT of A(z) */

codec2_decode_params(&dec->params, bits, p);   /* unpack, dequantise, interpolate */
codec2_params_to_model(dec, p, NULL, m, Aw);    /* LSP -> LPC -> amplitudes */
codec2_synthesise(dec, m, Aw, speech);          /* phases, post filter, overlap-add */
```

| Function | Purpose |
|---|---|
| `codec2_params_init(&st)` | parameter history with the values of `codec2_decoder_init()`; `codec2_params_state` has the fields of Codec2-mod-neural's `c2f_params_state` |
| `codec2_decode_params(&st, bits, p)` | the parameter half of `codec2_decode()` |
| `codec2_params_to_model(dec, p, &opts, m, Aw)` | `codec2_model_opts` {`post_filter`, `bass_boost`, `beta`, `gamma`, `lpc_correction`} with upstream `aks_to_M2()` semantics; `NULL` = defaults |
| `codec2_model_opts_default(&opts)` | defaults used by `codec2_decode()`: on, on, 0.2, 0.5, on |
| `codec2_synthesise(dec, m, Aw, speech)` | synthesis of both halves |
| `codec2_decoder_reseed(dec, seed)` | set the per-instance phase generator (init sets 1) |

Encoder side (building frames without PCM analysis):

| Function | Purpose |
|---|---|
| `codec2_pack_params(bits, v0, &p1)` | quantise and pack exactly as `codec2_encode()` does after analysis (`p1` = second half) |
| `codec2_levinson_durbin(R, ak)` | order 10 Levinson-Durbin, same math as the encoder |
| `codec2_lpc_bw_expand(ak)` | the encoder's 15 Hz bandwidth expansion |
| `codec2_lpc_to_lsp(ak, lsp)` | LPC to LSP with the encoder's root search; returns the root count, flat LSPs when it is not 10 |
| `CODEC2_WO_MIN`, `CODEC2_WO_MAX`, `codec2_wo_limits()` | Wo range of the 7-bit quantiser |

Misc: `codec2_mod_version()`, `codec2_size()`, `codec2_encoder_size()`,
`codec2_decoder_size()`, `codec2_state_alignment()`.

**Clean-header mode (opt-in).** Define `CODEC2_MOD_PUBLIC_ONLY` before
including `codec2_mod.h`: `codec2_internal.h` is then not included, no
unprefixed macro leaks (only `CODEC2_*`), the state types are opaque and the
header is C99 clean. Allocate the state yourself:

```c
#define CODEC2_MOD_PUBLIC_ONLY
#include "codec2_mod.h"

codec2_decoder_t *dec = aligned_alloc(codec2_state_alignment(),
                                      codec2_decoder_size()); /* size rounded up to the alignment if needed */
codec2_decoder_init(dec);
```

The public types live in `codec2_mod_types.h`, which the internal headers
include too, so `codec2_model_t` is `model_t` and `codec2_complex_t` is
`complex_t` in both modes.

**Symbol prefix (opt-in).** Building the library and its callers with
`-DCODEC2_MOD_PREFIX=c2m_` prefixes every global symbol (API and exported
internals), so the archive can be linked next to upstream `libcodec2`; see
`inc/codec2_mod_prefix.h`. Off by default.

**Build and tests.** `make` builds `build/libcodec2_mod.a`, the CLI pair
`c2m_enc` / `c2m_dec` (raw s16le 8 kHz <-> 8-byte frames, clean-header mode;
`c2m_dec` exposes the model options and the seed) and the tests;
`make check INPUT=speech.raw REF_BIT=speech.bit [REF_PCM=dec.raw]` checks the
encoder against a reference bitstream (e.g. from `c2enc 3200`), the decoder
layers, options, reseed and pack identities (`c2m_test`) and the clean-header
mode compiled as C99 (`c2m_public_only`).

## Important notice: derivative work

> [!NOTE]
> **This is a derivative work.**

This code is based heavily and directly on the Codec2 speech codec by venerable David Rowe, VK5DGR<sup>[1](https://github.com/drowe67) [2](https://www.qrz.com/db/VK5DGR)</sup> et al.

Original project:
- https://github.com/drowe67/codec2

Large portions of the code, algorithms, constants, and overall design originate from Codec2 and remain recognizably derived from it.  
All original credit for the Codec2 design, algorithms, and implementation belongs to David Rowe and the Codec2 contributors.

This repository exists to study, understand, optimize, and experimentally extend the Codec2 3200 bps mode.

## License

This project inherits the licensing requirements of Codec2.  
Please refer to the LICENSE file and original Codec2 license for details and ensure compliance when using or redistributing this code.

