cmake_minimum_required(VERSION 2.8)

project(tjost)

# pkg-config
find_package(PkgConfig REQUIRED)
include(CheckIncludeFiles)

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/tlsf-3.0")
include_directories("${PROJECT_SOURCE_DIR}/jackey")

# libuv
pkg_search_module(UV REQUIRED libuv)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# jack
pkg_search_module(JACK REQUIRED jack)
include_directories(${JACK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${JACK_LDFLAGS})

# metadata
CHECK_INCLUDE_FILES("jack/metadata.h" HAS_METADATA_API)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tjost_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/tjost_config.h @ONLY)

# alsa
pkg_search_module(ALSA REQUIRED alsa)
include_directories(${ALSA_INCLUDE_DIRS})

# lua
option(USE_LUAJIT "use LuaJIT" ON)
if(USE_LUAJIT)
	pkg_search_module(LUAJIT REQUIRED luajit)
	include_directories(${LUAJIT_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
else(USE_LUAJIT)
	pkg_search_module(LUA REQUIRED lua)
	include_directories(${LUA_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUA_LDFLAGS})
endif(USE_LUAJIT)

# eina
pkg_search_module(EINA REQUIRED eina)
include_directories(${EINA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EINA_LDFLAGS})

# shared lib
add_library(jack_osc SHARED jack_osc.c)
target_link_libraries(jack_osc ${JACK_LDFLAGS})
install(TARGETS jack_osc DESTINATION lib)

# lib header
install(FILES jack_osc.h DESTINATION include)

# pkg config file
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_PREFIX}/lib)
set(includedir ${CMAKE_INSTALL_PREFIX}/include)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/jack_osc.pc.in ${CMAKE_CURRENT_BINARY_DIR}/jack_osc.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jack_osc.pc DESTINATION lib/pkgconfig)

# tjost
add_executable(tjost tjost.c tjost_lua.c tjost_uplink.c tlsf-3.0/tlsf.c)
target_link_libraries(tjost jack_osc ${LIBS})
install(TARGETS tjost DESTINATION bin)

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

# mod_dump
add_library(dump MODULE mod_dump.c)
set_target_properties(dump PROPERTIES PREFIX "")
install(TARGETS dump DESTINATION lib/tjost)

# mod_send
add_library(send MODULE mod_send.c)
set_target_properties(send PROPERTIES PREFIX "")
install(TARGETS send DESTINATION lib/tjost)

# mod_midi_in
add_library(midi_in MODULE mod_midi_in.c)
set_target_properties(midi_in PROPERTIES PREFIX "")
install(TARGETS midi_in DESTINATION lib/tjost)

# mod_midi_out
add_library(midi_out MODULE mod_midi_out.c)
set_target_properties(midi_out PROPERTIES PREFIX "")
install(TARGETS midi_out DESTINATION lib/tjost)

# mod_raw_in
add_library(raw_in MODULE mod_raw_in.c)
set_target_properties(raw_in PROPERTIES PREFIX "")
target_link_libraries(raw_in ${ALSA_LDFLAGS})
install(TARGETS raw_in DESTINATION lib/tjost)

# mod_raw_out
add_library(raw_out MODULE mod_raw_out.c)
set_target_properties(raw_out PROPERTIES PREFIX "")
target_link_libraries(raw_out ${ALSA_LDFLAGS})
install(TARGETS raw_out DESTINATION lib/tjost)

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

# mod_audio_in
add_library(audio_in MODULE mod_audio_in.c)
set_target_properties(audio_in PROPERTIES PREFIX "")
install(TARGETS audio_in DESTINATION lib/tjost)

# mod_audio_out
add_library(audio_out MODULE mod_audio_out.c)
set_target_properties(audio_out PROPERTIES PREFIX "")
install(TARGETS audio_out DESTINATION lib/tjost)

# mod_cv_in
add_library(cv_in MODULE mod_cv_in.c)
set_target_properties(cv_in PROPERTIES PREFIX "")
install(TARGETS cv_in DESTINATION lib/tjost)

# mod_cv_out
add_library(cv_out MODULE mod_cv_out.c)
set_target_properties(cv_out PROPERTIES PREFIX "")
install(TARGETS cv_out DESTINATION lib/tjost)

# mod_net_out
add_library(net_out MODULE mod_net_out.c mod_net.c netaddr_udp.c netaddr_tcp.c)
set_target_properties(net_out PROPERTIES PREFIX "")
install(TARGETS net_out DESTINATION lib/tjost)

# mod_net_in
add_library(net_in MODULE mod_net_in.c mod_net.c netaddr_udp.c netaddr_tcp.c)
set_target_properties(net_in PROPERTIES PREFIX "")
install(TARGETS net_in DESTINATION lib/tjost)

# fltr_tuio2
add_library(tuio2 MODULE fltr_tuio2.c)
set_target_properties(tuio2 PROPERTIES PREFIX "")
install(TARGETS tuio2 DESTINATION lib/tjost/lua)

# fltr_midi
add_library(midi MODULE fltr_midi.c)
set_target_properties(midi PROPERTIES PREFIX "")
install(TARGETS midi DESTINATION lib/tjost/lua)

add_subdirectory(bin)
