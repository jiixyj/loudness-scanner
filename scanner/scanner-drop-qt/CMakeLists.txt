if(TARGET scanner-tag)
  set(CMAKE_AUTOMOC ON)

  find_package(Qt5 COMPONENTS Gui Svg Widgets)

  set(SUMMARY_QT5_FOUND
      ${Qt5_FOUND}
      CACHE INTERNAL "")

  if(Qt5_FOUND AND NOT DISABLE_QT5)
    add_executable(loudness-drop-qt5 scanner-drop-qt)

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LOGO_INCLUDE_DIR})
    target_link_libraries(
      loudness-drop-qt5
      scanner-tag
      filetree
      input
      ${GLIB20_LIBRARIES}
      ${GTHREAD20_LIBRARIES}
      Qt5::Gui
      Qt5::Svg
      Qt5::Widgets)
  endif()
endif()
