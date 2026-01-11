cmake_minimum_required(VERSION 3.10..4.1)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

project(
	micro-audio
	VERSION 1.0
	DESCRIPTION "An ultra-simple audio library focused on the KISS principles."
	LANGUAGES C
)

if (MSVC)
    # warning level 4 and all warnings as errors
    add_compile_options(/Wall /WX /wd4710 /wd4711 /wd5045 /wd4820)
	set_directory_properties(PROPERTIES COMPILE_DEFINITIONS $<$<CONFIG:Debug>:_DEBUG>)
	add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
else()
    # lots of warnings and all warnings as errors
    add_compile_options(-Wall -Wextra -Wconversion -Wno-sign-conversion -Wdouble-promotion -Wno-unused-parameter -Wno-unused-function)
endif()

option(BUILD_SHARED_LIBS "Turn on if you want to build dynamic libraries." OFF)
if(BUILD_SHARED_LIBS)
add_compile_definitions(BUILD_SHARED_LIBS)
endif()

add_library(micro-audio "ua_api.h" "ua_api.c")
if (MSVC)
	target_link_libraries(micro-audio PRIVATE XAudio2)
elseif (APPLE)
	find_library(COREFOUNDATION_LIBRARY CoreFoundation)
	find_library(COREAUDIO_LIBRARY CoreAudio)
	find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
	target_link_libraries(micro-audio PRIVATE ${COREFOUNDATION_LIBRARY} ${COREAUDIO_LIBRARY} ${AUDIOTOOLBOX_LIBRARY})
endif ()
set_target_properties(micro-audio PROPERTIES LINKER_LANGUAGE C)
