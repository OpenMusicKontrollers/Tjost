#configure_file("${PROJECT_SOURCE_DIR}/bin/tjost_midi2osc.in" "${PROJECT_SOURCE_DIR}/bin/tjost_midi2osc")

# convenience scripts
set(scripts "")

set(scripts ${scripts} tjost_midi2osc)
set(scripts ${scripts} tjost_osc2midi)
set(scripts ${scripts} tjost_audio2osc)
set(scripts ${scripts} tjost_osc2audio)
set(scripts ${scripts} tjost_osc2udp)
set(scripts ${scripts} tjost_udp2osc)
set(scripts ${scripts} tjost_dump)

install(PROGRAMS ${scripts} DESTINATION bin)
