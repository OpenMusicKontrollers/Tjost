cmake_minimum_required(VERSION 2.8)

project(tjost)

set (CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(LIBS "-lm")

# pkg-config
find_package(PkgConfig REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/tlsf")

# libuv
pkg_search_module(UV REQUIRED libuv)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# jack
pkg_search_module(JACK REQUIRED jack)
include_directories(${JACK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${JACK_LDFLAGS})

# lua
#option(USE_LUAJIT "use LuaJIT" OFF)
#if(USE_LUAJIT)
#	pkg_search_module(LUAJIT REQUIRED luajit)
#	include_directories(${LUAJIT_INCLUDE_DIRS})
#	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
#else(USE_LUAJIT)
#	pkg_search_module(LUA REQUIRED lua)
#	include_directories(${LUA_INCLUDE_DIRS})
#	set(LIBS ${LIBS} ${LUA_LDFLAGS})
#endif(USE_LUAJIT)
include_directories(${CMAKE_SOURCE_DIR}/LuaJIT-2.0.2/src)

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

# tjost
add_executable(tjost tjost.c tjost_lua.c tjost_uplink.c netaddr_udp.c netaddr_tcp.c tlsf/tlsf.c)
target_link_libraries(tjost jack_osc ${LIBS} "${CMAKE_SOURCE_DIR}/LuaJIT-2.0.2/src/libluajit.a" "-ldl")
install(TARGETS tjost DESTINATION bin)

# mod_uplink
add_library(uplink MODULE mod_uplink.c)
set_target_properties(uplink PROPERTIES PREFIX "")
install(TARGETS uplink DESTINATION lib/tjost)

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

# mod_audio_in
add_library(audio_in MODULE mod_audio_in.c)
set_target_properties(audio_in PROPERTIES PREFIX "")
install(TARGETS audio_in DESTINATION lib/tjost)

# mod_audio_out
add_library(audio_out MODULE mod_audio_out.c)
set_target_properties(audio_out PROPERTIES PREFIX "")
install(TARGETS audio_out DESTINATION lib/tjost)

# mod_net_out
add_library(net_out MODULE mod_net_out.c)
set_target_properties(net_out PROPERTIES PREFIX "")
install(TARGETS net_out DESTINATION lib/tjost)

# mod_net_in
add_library(net_in MODULE mod_net_in.c)
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
