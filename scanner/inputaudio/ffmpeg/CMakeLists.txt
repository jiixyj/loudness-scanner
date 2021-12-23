include(utils)

find_package(PkgConfig)
pkg_check_modules(LIBAVFORMAT libavformat)
pkg_check_modules(LIBAVCODEC libavcodec)
pkg_check_modules(LIBAVUTIL libavutil)
pkg_check_modules(GMODULE20 gmodule-2.0)

set(SUMMARY_FFMPEG_FOUND
    ${LIBAVFORMAT_FOUND}
    CACHE INTERNAL "")

if(LIBAVFORMAT_FOUND
   AND GMODULE20_FOUND
   AND INPUT_INCLUDE_DIR
   AND EBUR128_INCLUDE_DIR
   AND NOT DISABLE_GLIB20
   AND NOT DISABLE_FFMPEG)

  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
  include_directories(${LIBAVFORMAT_INCLUDE_DIRS} ${LIBAVUTIL_INCLUDE_DIRS}
                      ${GMODULE20_INCLUDE_DIRS})
  link_directories(${LIBAVFORMAT_LIBRARY_DIRS} ${LIBAVCODEC_LIBRARY_DIRS}
                   ${LIBAVUTIL_LIBRARY_DIRS} ${GMODULE20_LIBRARY_DIRS})

  add_library(input_ffmpeg MODULE input_ffmpeg.c ../input_helper.c)

  target_link_libraries(
    input_ffmpeg ${LIBAVFORMAT_LIBRARIES} ${LIBAVCODEC_LIBRARIES}
    ${LIBAVUTIL_LIBRARIES} ${GMODULE20_LIBRARIES})

  list(APPEND INPUT_FFMPEG_CFLAGS ${LIBAVFORMAT_CFLAGS_OTHER}
       ${GMODULE20_CFLAGS_OTHER})
  list(APPEND INPUT_FFMPEG_LDFLAGS ${LIBAVFORMAT_LDFLAGS_OTHER}
       ${LIBAVCODEC_LDFLAGS_OTHER} ${LIBAVUTIL_LDFLAGS_OTHER}
       ${GMODULE20_LDFLAGS_OTHER})

  if(INPUT_FFMPEG_CFLAGS)
    to_space_list(INPUT_FFMPEG_CFLAGS)
    set_target_properties(input_ffmpeg PROPERTIES COMPILE_FLAGS
                                                  ${INPUT_FFMPEG_CFLAGS})
  endif()
  if(INPUT_FFMPEG_LDFLAGS)
    to_space_list(INPUT_FFMPEG_LDFLAGS)
    set_target_properties(input_ffmpeg PROPERTIES LINK_FLAGS
                                                  ${INPUT_FFMPEG_LDFLAGS})
  endif()
endif()
