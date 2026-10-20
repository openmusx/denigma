add_denigma_internal_library(denigma_input MUSX_PCH input.cpp)
add_library(denigma::input ALIAS denigma_input)
target_link_libraries(denigma_input PUBLIC denigma_core finale_mus_reader::finale_mus_reader PRIVATE
    denigma_format_enigmaxml denigma_utils pugixml)
