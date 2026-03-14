cmake_minimum_required(VERSION 3.16)

project(Curio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic moc (and friends) so that signals/slots work.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

# Optional: AppStream Qt for rich metadata (description, screenshots). When not found, app works with CLI data only.
set(HAVE_APPSTREAM_QT OFF)
find_package(AppStreamQt QUIET)
if(AppStreamQt_FOUND)
  # Target name varies by distro: Meson/Arch often exports AppStreamQt::AppStreamQt
  if(TARGET AppStreamQt::AppStreamQt)
    set(APPSTREAM_QT_TARGET AppStreamQt::AppStreamQt)
    set(HAVE_APPSTREAM_QT ON)
  elseif(TARGET AppStream::AppStreamQt)
    set(APPSTREAM_QT_TARGET AppStream::AppStreamQt)
    set(HAVE_APPSTREAM_QT ON)
  elseif(TARGET AppStreamQt)
    set(APPSTREAM_QT_TARGET AppStreamQt)
    set(HAVE_APPSTREAM_QT ON)
  endif()
endif()
if(NOT HAVE_APPSTREAM_QT)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(APPSTREAM_QT QUIET appstream-qt)
    if(APPSTREAM_QT_FOUND)
      set(HAVE_APPSTREAM_QT ON)
      set(APPSTREAM_QT_USE_PKGCONFIG ON)
    endif()
  endif()
endif()

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(SOURCES
    ${SRC_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
    ${SRC_DIR}/ui/MainWindow.cpp
    ${SRC_DIR}/ui/AppItemDelegate.cpp
    ${SRC_DIR}/ui/AppDetailsWidget.cpp
    ${SRC_DIR}/ui/AppCardWidget.cpp
    ${SRC_DIR}/ui/InstalledRowWidget.cpp
    ${SRC_DIR}/models/AppListModel.cpp
    ${SRC_DIR}/models/OperationModel.cpp
    ${SRC_DIR}/backend/FlatpakBackend.cpp
    ${SRC_DIR}/backend/AppStreamProvider.cpp
)

set(HEADERS
    ${SRC_DIR}/ui/MainWindow.h
    ${SRC_DIR}/ui/AppItemDelegate.h
    ${SRC_DIR}/ui/AppDetailsWidget.h
    ${SRC_DIR}/ui/AppCardWidget.h
    ${SRC_DIR}/ui/InstalledRowWidget.h
    ${SRC_DIR}/models/AppInfo.h
    ${SRC_DIR}/models/AppListModel.h
    ${SRC_DIR}/models/Operation.h
    ${SRC_DIR}/models/OperationModel.h
    ${SRC_DIR}/backend/FlatpakBackend.h
    ${SRC_DIR}/backend/AppStreamProvider.h
)

add_executable(curio
    ${SOURCES}
    ${HEADERS}
)

target_compile_definitions(curio PRIVATE
    $<$<BOOL:${HAVE_APPSTREAM_QT}>:HAVE_APPSTREAM_QT=1>
)

target_include_directories(curio PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ui
    ${SRC_DIR}/models
    ${SRC_DIR}/backend
)

target_link_libraries(curio PRIVATE Qt6::Widgets Qt6::Network)
if(HAVE_APPSTREAM_QT)
  if(DEFINED APPSTREAM_QT_TARGET)
    target_link_libraries(curio PRIVATE ${APPSTREAM_QT_TARGET})
  elseif(APPSTREAM_QT_USE_PKGCONFIG)
    target_include_directories(curio PRIVATE ${APPSTREAM_QT_INCLUDE_DIRS})
    target_link_directories(curio PRIVATE ${APPSTREAM_QT_LIBRARY_DIRS})
    target_link_libraries(curio PRIVATE ${APPSTREAM_QT_LIBRARIES})
  endif()
endif()

install(TARGETS curio RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/curio.desktop
        DESTINATION share/applications)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME curio.png)

