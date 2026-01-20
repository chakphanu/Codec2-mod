# ESP-IDF Component CMakeLists.txt for Codec2-mod
# Optimized for ESP32-S3 with esp-dsp SIMD acceleration

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
)

set(CODEC2_INCLUDES
    "inc"
)

# Check if we're building as an ESP-IDF component
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${CODEC2_SRCS}
        INCLUDE_DIRS ${CODEC2_INCLUDES}
        REQUIRES esp-dsp
    )

    # ESP32-S3 specific optimizations
    if(CONFIG_IDF_TARGET_ESP32S3)
        # Enable esp-dsp SIMD optimizations
        target_compile_definitions(${COMPONENT_LIB} PUBLIC
            CODEC2_ESP32S3_DSP=1
        )

        # Optimize for speed on DSP-heavy code
        target_compile_options(${COMPONENT_LIB} PRIVATE
            -O2
            -ffunction-sections
            -fdata-sections
        )
    endif()

    # Common ESP32 definitions
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        CODEC2_ESP_PLATFORM=1
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
