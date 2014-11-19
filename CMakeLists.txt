cmake_minimum_required(VERSION 3.0)

project(tjost)

# pkg-config
find_package(PkgConfig REQUIRED)
include(CheckIncludeFiles)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_SOURCE_DIR}/tlsf-3.0)
include_directories(${PROJECT_SOURCE_DIR}/modules)
include_directories(${PROJECT_SOURCE_DIR}/jackey)
include_directories(${PROJECT_SOURCE_DIR}/libosc)
include_directories(${PROJECT_SOURCE_DIR}/libosc_stream)
include_directories(${PROJECT_SOURCE_DIR}/jack_osc)

# libuv
pkg_search_module(UV REQUIRED libuv>=1.0.0)
include_directories(${UV_INCLUDE_DIRS})
set(LIBS ${LIBS} ${UV_LDFLAGS})

# jack
pkg_search_module(JACK REQUIRED jack>=0.124.1)
include_directories(${JACK_INCLUDE_DIRS})
set(LIBS ${LIBS} ${JACK_LDFLAGS})

# metadata
CHECK_INCLUDE_FILES("jack/metadata.h" HAS_METADATA_API)

# alsa
option(USE_ALSA "use ALSA" ON)
if(USE_ALSA)
	pkg_search_module(ALSA REQUIRED alsa>=1.0.28)
	include_directories(${ALSA_INCLUDE_DIRS})
endif()

# RtMidi
option(USE_RTMIDI "use RtMidi" ON)
if(USE_RTMIDI)
	pkg_search_module(RTMIDI REQUIRED librtmidi>=2.1.0)
	include_directories(${RTMIDI_INCLUDE_DIRS})
endif()

# lua
option(USE_LUAJIT "use LuaJIT" ON)
if(USE_LUAJIT)
	pkg_search_module(LUAJIT REQUIRED luajit>=2.0.3)
	include_directories(${LUAJIT_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUAJIT_LDFLAGS})
else()
	pkg_search_module(LUA REQUIRED lua)
	include_directories(${LUA_INCLUDE_DIRS})
	set(LIBS ${LIBS} ${LUA_LDFLAGS})
endif()

# configure file
configure_file(${PROJECT_SOURCE_DIR}/tjost_config.h.in ${PROJECT_BINARY_DIR}/tjost_config.h @ONLY)

# eina
pkg_search_module(EINA REQUIRED eina>=1.11.2)
include_directories(${EINA_INCLUDE_DIRS})
set(LIBS ${LIBS} ${EINA_LDFLAGS})

# osc
add_library(osc libosc/osc.c)

# osc_stream
add_library(osc_stream
	libosc_stream/osc_stream.c
	libosc_stream/osc_stream_pipe.c
	libosc_stream/osc_stream_slip.c
	libosc_stream/osc_stream_tcp.c
	libosc_stream/osc_stream_udp.c)

# tlsf
add_library(tlsf tlsf-3.0/tlsf.c)

# tjost
add_executable(tjost
	tjost.c
	tjost_lua.c
	tjost_uplink.c
	tjost_nsm.c
	tjost_pipe.c)
target_link_libraries(tjost osc osc_stream tlsf ${LIBS})
install(TARGETS tjost DESTINATION bin)

add_subdirectory(bin)
add_subdirectory(modules)
