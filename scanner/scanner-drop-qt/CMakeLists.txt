if(TARGET scanner-tag)


  find_package(Qt4 COMPONENTS QtGui QtSvg)

  set(SUMMARY_QT4_FOUND  ${Qt4_FOUND}  CACHE INTERNAL "")

  if(Qt4_FOUND AND NOT DISABLE_QT4)
    add_executable(loudness-drop-qt scanner-drop-qt moc_scanner-drop-qt4.cpp)
    QT4_GENERATE_MOC(scanner-drop-qt.h moc_scanner-drop-qt4.cpp loudness-drop-qt)

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LOGO_INCLUDE_DIR})
    target_link_libraries(loudness-drop-qt
            scanner-tag filetree input
            ${GLIB20_LIBRARIES}
            ${GTHREAD20_LIBRARIES}
            Qt4::QtGui
            Qt4::QtSvg
            )
  endif()


  find_package(Qt5 COMPONENTS Gui Svg Widgets)

  set(SUMMARY_QT5_FOUND  ${Qt5_FOUND}  CACHE INTERNAL "")

  if(Qt5_FOUND AND NOT DISABLE_QT5)
    add_executable(loudness-drop-qt5 scanner-drop-qt moc_scanner-drop-qt5.cpp)
    QT5_GENERATE_MOC(scanner-drop-qt.h moc_scanner-drop-qt5.cpp loudness-drop-qt5)

    include_directories(${CMAKE_CURRENT_BINARY_DIR} ${LOGO_INCLUDE_DIR})
    target_link_libraries(loudness-drop-qt5
            scanner-tag filetree input
            ${GLIB20_LIBRARIES}
            ${GTHREAD20_LIBRARIES}
            Qt5::Gui
            Qt5::Svg
            Qt5::Widgets
            )
  endif()
endif()
