cmake_minimum_required(VERSION 2.8.9)

include(utils)

find_package(SNDFILE)
find_package(PkgConfig)
pkg_check_modules(GMODULE20 gmodule-2.0)

if(GMODULE20_FOUND
   AND SNDFILE_FOUND
   AND INPUT_INCLUDE_DIR
   AND EBUR128_INCLUDE_DIR
   AND NOT DISABLE_GLIB20
   AND NOT DISABLE_SNDFILE)
  include_directories(SYSTEM ${SNDFILE_INCLUDE_DIRS})
  include_directories(${INPUT_INCLUDE_DIR} ${EBUR128_INCLUDE_DIR})
  include_directories(${GMODULE20_INCLUDE_DIRS})
  link_directories(${SNDFILE_LIBRARY_DIRS} ${GMODULE20_LIBRARY_DIRS})

  add_library(input_sndfile MODULE input_sndfile.c ../input_helper.c)

  target_link_libraries(input_sndfile ${SNDFILE_LIBRARIES}
                        ${GMODULE20_LIBRARIES})

  list(APPEND INPUT_SNDFILE_CFLAGS ${GMODULE20_CFLAGS_OTHER})
  list(APPEND INPUT_SNDFILE_LDFLAGS ${GMODULE20_LDFLAGS_OTHER})

  if(INPUT_SNDFILES_CFLAGS)
    to_space_list(INPUT_SNDFILE_CFLAGS)
    set_target_properties(input_sndfile PROPERTIES COMPILE_FLAGS
                                                   ${INPUT_SNDFILE_CFLAGS})
  endif()
  if(INPUT_SNDFILE_LDFLAGS)
    to_space_list(INPUT_SNDFILE_LDFLAGS)
    set_target_properties(input_sndfile PROPERTIES LINK_FLAGS
                                                   ${INPUT_SNDFILE_LDFLAGS})
  endif()
endif()
