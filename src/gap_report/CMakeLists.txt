# The gap report library serializes collected conversion gaps as JSON. Exporters
# never link it: they depend only on the classify library's GapCollector. Link
# this target from a front end (CLI, WASM, tests) that writes reports.
add_denigma_internal_library(denigma_gap_report
    ${CMAKE_CURRENT_LIST_DIR}/gap_report.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gap_report_chords.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gap_report_expressions.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gap_report_formatted_text.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gap_report_noteheads.cpp
    ${CMAKE_CURRENT_LIST_DIR}/gap_report_smartshapes.cpp
)
add_library(denigma::gap-report ALIAS denigma_gap_report)
target_link_libraries(denigma_gap_report
    PUBLIC denigma_classify
    PRIVATE nlohmann_json::nlohmann_json
)
target_compile_definitions(denigma_gap_report INTERFACE DENIGMA_HAS_GAP_REPORT=1)
