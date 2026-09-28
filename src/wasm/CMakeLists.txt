# WebAssembly module exposing the MUSX inspection and conversion entry points
# through a small C ABI. Denigma Online consumes the denigma.js/denigma.wasm pair
# this target emits, so the exported function list below is that site's contract.

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "denigma_BUILD_WASM requires the Emscripten toolchain (configure with emcmake).")
endif()

add_executable(denigma_wasm denigma_wasm.cpp)
target_compile_options(denigma_wasm PRIVATE ${DENIGMA_WARNING_OPTIONS})
target_link_libraries(denigma_wasm PRIVATE
    denigma::enigmaxml
    denigma::mnx
    denigma::musicxml
    denigma_internal_deps
)
if(denigma_BUILD_GAP_REPORT)
    target_link_libraries(denigma_wasm PRIVATE denigma::gap-report)
endif()

set(DENIGMA_WASM_OUTPUT_DIR "${PROJECT_BINARY_DIR}/wasm")
set_target_properties(denigma_wasm PROPERTIES
    OUTPUT_NAME denigma
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${DENIGMA_WASM_OUTPUT_DIR}"
)

set(_denigma_wasm_exports
    _denigma_malloc
    _denigma_free
    _denigma_inspect
    _denigma_convert
    _denigma_result_destroy
    _denigma_result_success
    _denigma_result_diagnostic_count
    _denigma_result_diagnostic_severity
    _denigma_result_diagnostic_message
    _denigma_result_score_name
    _denigma_result_score_page_width_mm
    _denigma_result_score_page_height_mm
    _denigma_result_score_spatium_mm
    _denigma_result_score_has_page_margins
    _denigma_result_score_page_margin_top_sp
    _denigma_result_score_page_margin_bottom_sp
    _denigma_result_score_page_margin_left_sp
    _denigma_result_score_page_margin_right_sp
    _denigma_result_part_count
    _denigma_result_part_id
    _denigma_result_part_name
    _denigma_result_part_output_index
    _denigma_result_part_page_width_mm
    _denigma_result_part_page_height_mm
    _denigma_result_part_spatium_mm
    _denigma_result_part_has_page_margins
    _denigma_result_part_page_margin_top_sp
    _denigma_result_part_page_margin_bottom_sp
    _denigma_result_part_page_margin_left_sp
    _denigma_result_part_page_margin_right_sp
    _denigma_result_output_count
    _denigma_result_output_name
    _denigma_result_output_data
    _denigma_result_output_size
    _denigma_result_output_index
    _denigma_result_gap_report_data
    _denigma_result_gap_report_size
    _denigma_version
    _denigma_commit
    _denigma_mnx_schema_version
)
list(JOIN _denigma_wasm_exports "," _denigma_wasm_exports)

# Emscripten's default stack is 64 KiB, and MusicXML export needs about 80 KiB wherever
# mx writes ornaments, whatever the size of the score. A WebAssembly stack overflow is
# not trapped: it silently overwrites the static data beside the stack, including the
# C++ runtime's locale, and every later conversion fails. The larger stack leaves ample
# headroom, and the overflow check turns any overflow that remains into an abort that
# reports it, so the caller can discard the module instead of reusing a corrupted one.
set(_denigma_wasm_stack_size 1048576)

# Optimize for speed. Conversion time on large scores is dominated by building the
# musxdom document and running the exporters, which -O3 runs about twice as fast as
# a size-optimized build, for a compressed module roughly a fifth larger.
target_compile_options(denigma_wasm PRIVATE -O3)
target_link_options(denigma_wasm PRIVATE
    -O3
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sENVIRONMENT=web,worker,node"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sSTACK_SIZE=${_denigma_wasm_stack_size}"
    "-sSTACK_OVERFLOW_CHECK=2"
    "-sFILESYSTEM=0"
    "-sASSERTIONS=0"
    "-sEXPORTED_FUNCTIONS=${_denigma_wasm_exports}"
    "-sEXPORTED_RUNTIME_METHODS=HEAPU8,UTF8ToString"
)
unset(_denigma_wasm_exports)
unset(_denigma_wasm_stack_size)
