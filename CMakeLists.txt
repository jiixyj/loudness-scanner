cmake_minimum_required(VERSION 3.10)
project(loudness-scanner)

option(ENABLE_COMPILER_WARNINGS "enable compiler warnings" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include(utils)
include(GNUInstallDirs)

set(ENABLE_INTERNAL_QUEUE_H
    OFF
    CACHE BOOL "Use own queue.h")
set(DISABLE_GLIB20
    OFF
    CACHE BOOL "Don't build with glib")
set(DISABLE_SNDFILE
    OFF
    CACHE BOOL "Don't build with sndfile")
set(DISABLE_TAGLIB
    OFF
    CACHE BOOL "Don't build with taglib")
set(DISABLE_FFMPEG
    OFF
    CACHE BOOL "Don't build with ffmpeg")
set(DISABLE_RSVG2
    OFF
    CACHE BOOL "Don't build with rsvg2")
set(DISABLE_GTK2
    OFF
    CACHE BOOL "Don't build with gtk2")
set(DISABLE_QT5
    OFF
    CACHE BOOL "Don't build with qt5")

add_subdirectory(ebur128/ebur128)

if(ENABLE_COMPILER_WARNINGS)
  add_compile_options(
    -Wall #
    -Wextra #
    -Wconversion #
    -Wsign-conversion #
    -Wundef #
    -Werror=return-type #
    -Werror=undef #
    -Wno-unknown-pragmas)

  add_compile_options(
    $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Werror=incompatible-pointer-types>)

  add_compile_options(-fdiagnostics-color=always)

  add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-missing-field-initializers> #
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-builtin-declaration-mismatch> #
  )
endif()

add_subdirectory(scanner)

to_yes_no(
  SUMMARY_HAS_QUEUE
  SUMMARY_GLIB20_FOUND
  SUMMARY_GTHREAD20_FOUND
  SUMMARY_SNDFILE_FOUND
  SUMMARY_TAGLIB_FOUND
  SUMMARY_FFMPEG_FOUND
  SUMMARY_RSVG2_FOUND
  SUMMARY_GTK2_FOUND
  SUMMARY_QT5_FOUND)
to_yes_no(
  DISABLE_GLIB20
  DISABLE_SNDFILE
  DISABLE_TAGLIB
  DISABLE_FFMPEG
  DISABLE_RSVG2
  DISABLE_GTK2
  DISABLE_QT5)

if(ENABLE_INTERNAL_QUEUE_H)
  set(USE_QUEUE "using own copy of queue.h")
else()
  set(USE_QUEUE "using system copy of queue.h")
endif()

##### Print status
message(STATUS "status          found / disabled --")
message(STATUS "queue.h:        ${SUMMARY_HAS_QUEUE}" #
               "     ${USE_QUEUE}")
message(STATUS "glib-2.0:       ${SUMMARY_GLIB20_FOUND}"
               "     ${DISABLE_GLIB20}")
message(STATUS "gthread-2.0:    ${SUMMARY_GTHREAD20_FOUND}"
               "     ${DISABLE_GLIB20}")
message(STATUS "sndfile:        ${SUMMARY_SNDFILE_FOUND}"
               "     ${DISABLE_SNDFILE}")
message(STATUS "taglib:         ${SUMMARY_TAGLIB_FOUND}"
               "     ${DISABLE_TAGLIB}")
message(STATUS "ffmpeg:         ${SUMMARY_FFMPEG_FOUND}"
               "     ${DISABLE_FFMPEG}")
message(STATUS "rsvg2:          ${SUMMARY_RSVG2_FOUND}" #
               "     ${DISABLE_RSVG2}")
message(STATUS "gtk2:           ${SUMMARY_GTK2_FOUND}" #
               "     ${DISABLE_GTK2}")
message(STATUS "qt5:            ${SUMMARY_QT5_FOUND}" #
               "     ${DISABLE_QT5}")

if(NOT SUMMARY_HAS_QUEUE AND NOT ENABLE_INTERNAL_QUEUE_H)
  message(
    FATAL_ERROR "queue.h not found, please set ENABLE_INTERNAL_QUEUE_H to ON")
endif()
