# ESP-IDF Component CMakeLists.txt for Codec2-mod
# Supports pluggable DSP backends (reference C, ESP-DSP SIMD)

set(CODEC2_SRCS
    "src/analysis.c"
    "src/codec2_mod.c"
    "src/delta_lsp_cb.c"
    "src/interp.c"
    "src/kiss_fft.c"
    "src/kiss_fftr.c"
    "src/lpc.c"
    "src/nlp.c"
    "src/quantise.c"
    "src/synthesis.c"
    "src/util.c"
    "src/selftest.c"
    "src/dsp/dsp_ref.c"
    "src/dsp/dsp_espdsp.c"
)

set(CODEC2_INCLUDES
    "inc"
)

# Check if we're building as an ESP-IDF component
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${CODEC2_SRCS}
        INCLUDE_DIRS ${CODEC2_INCLUDES}
        PRIV_REQUIRES esp-dsp
    )

    # Optimize for speed on DSP-heavy code
    target_compile_options(${COMPONENT_LIB} PRIVATE
        -O2
        -ffunction-sections
        -fdata-sections
    )
else()
    # Standalone CMake build (non-ESP-IDF)
    cmake_minimum_required(VERSION 3.10)
    project(codec2_mod C)

    add_library(codec2_mod STATIC ${CODEC2_SRCS})
    target_include_directories(codec2_mod PUBLIC ${CODEC2_INCLUDES})
    target_compile_options(codec2_mod PRIVATE -O2 -Wall)

    # Link math library on non-ESP platforms
    target_link_libraries(codec2_mod m)
endif()
