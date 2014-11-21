# mod_uplink
add_library(uplink MODULE mod_uplink.c)
set_target_properties(uplink PROPERTIES PREFIX "")
install(TARGETS uplink DESTINATION lib/tjost)

# mod_timer
add_library(timer MODULE mod_timer.c)
set_target_properties(timer PROPERTIES PREFIX "")
install(TARGETS timer DESTINATION lib/tjost)

# mod_loopback
add_library(loopback MODULE mod_loopback.c)
set_target_properties(loopback PROPERTIES PREFIX "")
install(TARGETS loopback DESTINATION lib/tjost)

# mod_osc_in
add_library(osc_in MODULE mod_osc_in.c)
set_target_properties(osc_in PROPERTIES PREFIX "")
install(TARGETS osc_in DESTINATION lib/tjost)

# mod_osc_out
add_library(osc_out MODULE mod_osc_out.c)
set_target_properties(osc_out PROPERTIES PREFIX "")
install(TARGETS osc_out DESTINATION lib/tjost)

# mod_transport
add_library(transport MODULE mod_transport.c)
set_target_properties(transport PROPERTIES PREFIX "")
install(TARGETS transport DESTINATION lib/tjost)

# mod_dump
add_library(dump MODULE mod_dump.c)
set_target_properties(dump PROPERTIES PREFIX "")
install(TARGETS dump DESTINATION lib/tjost)

# mod_send
add_library(send MODULE mod_send.c)
set_target_properties(send PROPERTIES PREFIX "")
install(TARGETS send DESTINATION lib/tjost)

# mod_serial
add_library(serial MODULE mod_serial.c)
set_target_properties(serial PROPERTIES PREFIX "")
install(TARGETS serial DESTINATION lib/tjost)

# mod_write
add_library(write MODULE mod_write.c)
set_target_properties(write PROPERTIES PREFIX "")
install(TARGETS write DESTINATION lib/tjost)

# mod_read
add_library(read MODULE mod_read.c)
set_target_properties(read PROPERTIES PREFIX "")
install(TARGETS read DESTINATION lib/tjost)

# mod_midi_in
add_library(midi_in MODULE mod_midi_in.c)
set_target_properties(midi_in PROPERTIES PREFIX "")
install(TARGETS midi_in DESTINATION lib/tjost)

# mod_midi_out
add_library(midi_out MODULE mod_midi_out.c)
set_target_properties(midi_out PROPERTIES PREFIX "")
install(TARGETS midi_out DESTINATION lib/tjost)

if(USE_ALSA)
	# mod_seq_in
	add_library(seq_in MODULE mod_seq_in.c)
	set_target_properties(seq_in PROPERTIES PREFIX "")
	target_link_libraries(seq_in ${ALSA_LDFLAGS})
	install(TARGETS seq_in DESTINATION lib/tjost)

	# mod_seq_out
	add_library(seq_out MODULE mod_seq_out.c)
	set_target_properties(seq_out PROPERTIES PREFIX "")
	target_link_libraries(seq_out ${ALSA_LDFLAGS})
	install(TARGETS seq_out DESTINATION lib/tjost)
endif()

if(USE_RTMIDI)
	# mod_rtmidi_in
	add_library(rtmidi_in MODULE mod_rtmidi_in.c rtmidi_c.cpp)
	set_target_properties(rtmidi_in PROPERTIES PREFIX "")
	target_link_libraries(rtmidi_in ${RTMIDI_LDFLAGS})
	install(TARGETS rtmidi_in DESTINATION lib/tjost)

	# mod_rtmidi_out
	add_library(rtmidi_out MODULE mod_rtmidi_out.c rtmidi_c.cpp)
	set_target_properties(rtmidi_out PROPERTIES PREFIX "")
	target_link_libraries(rtmidi_out ${RTMIDI_LDFLAGS})
	install(TARGETS rtmidi_out DESTINATION lib/tjost)
endif()

# mod_cv_in
add_library(cv_in MODULE mod_cv_in.c)
set_target_properties(cv_in PROPERTIES PREFIX "")
install(TARGETS cv_in DESTINATION lib/tjost)

# mod_cv_out
add_library(cv_out MODULE mod_cv_out.c)
set_target_properties(cv_out PROPERTIES PREFIX "")
install(TARGETS cv_out DESTINATION lib/tjost)

# mod_net_out
add_library(net_out MODULE mod_net_out.c mod_net.c)
set_target_properties(net_out PROPERTIES PREFIX "")
install(TARGETS net_out DESTINATION lib/tjost)

# mod_net_in
add_library(net_in MODULE mod_net_in.c mod_net.c)
set_target_properties(net_in PROPERTIES PREFIX "")
install(TARGETS net_in DESTINATION lib/tjost)
