set(DENIGMA_EXPORT_COMMAND_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/export.cpp
)

add_denigma_internal_library(denigma_export MUSX_PCH ${DENIGMA_EXPORT_COMMAND_SOURCES})
# Export is a CLI orchestration layer and intentionally aggregates all format
# libraries. External projects that need one converter should link that converter
# directly instead of linking denigma_export.
target_link_libraries(denigma_export PUBLIC
    denigma_core
    denigma_format_enigmaxml
    denigma_musicxml
    denigma_mnx
    denigma_mss
    denigma_svg
)
if(denigma_BUILD_GAP_REPORT)
    # The gap report embeds SVG images whose text the CLI measures with the same backend as the SVG converter.
    target_link_libraries(denigma_export PUBLIC denigma_gap_report PRIVATE denigma_textmetrics)
endif()

if(denigma_BUILD_TESTING)
    add_denigma_internal_test_library(denigma_export_test ${DENIGMA_EXPORT_COMMAND_SOURCES})
    target_link_libraries(denigma_export_test PUBLIC
        denigma_core_test
        denigma_format_enigmaxml
        denigma_musicxml
        denigma_mnx
        denigma_mss
        denigma_svg
    )
endif()
if(denigma_BUILD_TESTING AND denigma_BUILD_GAP_REPORT)
    target_link_libraries(denigma_export_test PUBLIC denigma_gap_report PRIVATE denigma_textmetrics)
endif()
