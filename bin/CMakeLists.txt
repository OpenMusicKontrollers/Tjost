# convenience scripts

install (FILES tjost_cli.lua DESTINATION bin)

set(scripts "")
set(scripts ${scripts} tjost_midi2osc)
set(scripts ${scripts} tjost_osc2midi)
set(scripts ${scripts} tjost_cv2osc)
set(scripts ${scripts} tjost_osc2cv)
set(scripts ${scripts} tjost_osc2net)
set(scripts ${scripts} tjost_net2osc)
set(scripts ${scripts} tjost_dump)
set(scripts ${scripts} tjost_send)
set(scripts ${scripts} tjost_console)
set(scripts ${scripts} tjost_console2net)
set(scripts ${scripts} tjost_transport)
set(scripts ${scripts} tjost_write)
set(scripts ${scripts} tjost_read)
set(scripts ${scripts} tjost_serial)

if(USE_ALSA)
	set(scripts ${scripts} tjost_seq2osc)
	set(scripts ${scripts} tjost_osc2seq)
endif()

if(USE_RTMIDI)
	set(scripts ${scripts} tjost_rtmidi2osc)
	set(scripts ${scripts} tjost_osc2rtmidi)
endif()

install(PROGRAMS ${scripts} DESTINATION bin)
