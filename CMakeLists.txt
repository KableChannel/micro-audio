cmake_minimum_required(VERSION 3.10..4.1)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

project(
	micro-audio
	VERSION 1.0
	DESCRIPTION "An ultra-simple audio library focused on the KISS principles."
	LANGUAGES C
)

option(BUILD_SHARED_LIBS "Turn on if you want to build dynamic libraries." OFF)
if(BUILD_SHARED_LIBS)
add_compile_definitions(BUILD_SHARED_LIBS)
endif()

add_library(micro-audio "ua_api.h" "ua_api.c")
target_link_libraries(micro-audio PRIVATE XAudio2)
set_target_properties(micro-audio PROPERTIES LINKER_LANGUAGE C)