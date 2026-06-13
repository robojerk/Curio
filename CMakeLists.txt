cmake_minimum_required(VERSION 3.16)

project(Curio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt's automatic moc (and friends) so that signals/slots work.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

option(CURIO_SANDBOXED_LIBFLATPAK "Build for Curio running inside its own Flatpak sandbox" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLATPAK REQUIRED flatpak>=1.9)

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
    ${SRC_DIR}/ui/RuntimeUpdatesRowWidget.cpp
    ${SRC_DIR}/models/AppListModel.cpp
    ${SRC_DIR}/models/OperationModel.cpp
    ${SRC_DIR}/backend/FlatpakBackend.cpp
    ${SRC_DIR}/backend/FlatpakInstallationService.cpp
    ${SRC_DIR}/backend/FlatpakRefMapper.cpp
    ${SRC_DIR}/backend/FlatpakTransactionRunner.cpp
    ${SRC_DIR}/backend/FlatpakRemoteCatalog.cpp
    ${SRC_DIR}/backend/AppDisplayNames.cpp
    ${SRC_DIR}/backend/NetworkAccessUtils.cpp
    ${SRC_DIR}/backend/FlathubApiClient.cpp
    ${SRC_DIR}/backend/AppStreamProvider.cpp
    ${SRC_DIR}/backend/GitHost.cpp
    ${SRC_DIR}/backend/TrackedBuildSource.cpp
    ${SRC_DIR}/backend/TrackedBuildClassifier.cpp
    ${SRC_DIR}/backend/CurioSettings.cpp
    ${SRC_DIR}/backend/TrackedBuildMatcher.cpp
    ${SRC_DIR}/backend/CatalogCache.cpp
    ${SRC_DIR}/backend/SandboxedFlatpakEnv.cpp
    ${SRC_DIR}/ui/TrackedBuildSetupDialog.cpp
)

set(HEADERS
    ${SRC_DIR}/ui/MainWindow.h
    ${SRC_DIR}/ui/AppItemDelegate.h
    ${SRC_DIR}/ui/AppDetailsWidget.h
    ${SRC_DIR}/ui/AppCardWidget.h
    ${SRC_DIR}/ui/InstalledRowWidget.h
    ${SRC_DIR}/ui/RuntimeUpdatesRowWidget.h
    ${SRC_DIR}/models/AppInfo.h
    ${SRC_DIR}/models/StoreTemplate.h
    ${SRC_DIR}/models/TrackedBuild.h
    ${SRC_DIR}/models/AppListModel.h
    ${SRC_DIR}/models/Operation.h
    ${SRC_DIR}/models/OperationModel.h
    ${SRC_DIR}/backend/FlatpakBackend.h
    ${SRC_DIR}/backend/FlatpakInstallationService.h
    ${SRC_DIR}/backend/FlatpakRefMapper.h
    ${SRC_DIR}/backend/FlatpakScope.h
    ${SRC_DIR}/backend/FlatpakGlibInclude.h
    ${SRC_DIR}/backend/FlatpakTransactionRunner.h
    ${SRC_DIR}/backend/FlatpakRemoteCatalog.h
    ${SRC_DIR}/backend/NetworkAccessUtils.h
    ${SRC_DIR}/backend/FlathubApiClient.h
    ${SRC_DIR}/backend/AppStreamProvider.h
    ${SRC_DIR}/backend/GitHost.h
    ${SRC_DIR}/backend/TrackedBuildSource.h
    ${SRC_DIR}/backend/TrackedBuildClassifier.h
    ${SRC_DIR}/backend/CurioSettings.h
    ${SRC_DIR}/backend/TrackedBuildMatcher.h
    ${SRC_DIR}/backend/CatalogCache.h
    ${SRC_DIR}/backend/SandboxedFlatpakEnv.h
    ${SRC_DIR}/ui/TrackedBuildSetupDialog.h
)

add_executable(curio
    ${SOURCES}
    ${HEADERS}
)

target_compile_definitions(curio PRIVATE
    $<$<BOOL:${HAVE_APPSTREAM_QT}>:HAVE_APPSTREAM_QT=1>
    $<$<BOOL:${CURIO_SANDBOXED_LIBFLATPAK}>:CURIO_SANDBOXED_LIBFLATPAK=1>
)

target_include_directories(curio PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ui
    ${SRC_DIR}/models
    ${SRC_DIR}/backend
)

target_include_directories(curio PRIVATE ${FLATPAK_INCLUDE_DIRS})
target_link_directories(curio PRIVATE ${FLATPAK_LIBRARY_DIRS})
target_link_libraries(curio PRIVATE Qt6::Widgets Qt6::Network Qt6::Concurrent ${FLATPAK_LIBRARIES})
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
        DESTINATION share/applications
        RENAME io.github.curio.Curio.desktop)
file(GLOB CURIO_ICON_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/*/apps/io.github.curio.Curio.png")
if(CURIO_ICON_FILES)
  foreach(icon_file IN LISTS CURIO_ICON_FILES)
    string(REGEX REPLACE ".*/hicolor/([0-9]+)x([0-9]+)/apps/.*" "\\1" icon_px "${icon_file}")
    if(icon_px GREATER 512)
      message(WARNING "Skipping ${icon_file}: Flatpak allows hicolor icons up to 512x512")
      continue()
    endif()
    string(REGEX REPLACE ".*/hicolor/([0-9]+x[0-9]+)/apps/.*" "\\1" icon_size "${icon_file}")
    install(FILES "${icon_file}"
            DESTINATION "share/icons/hicolor/${icon_size}/apps")
  endforeach()
else()
  message(WARNING "No hicolor icons found; run: ./research/scripts/resize_icons.sh")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png
          DESTINATION share/icons/hicolor/512x512/apps
          RENAME io.github.curio.Curio.png)
endif()
set(CURIO_PIXMAP "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/hicolor/512x512/apps/io.github.curio.Curio.png")
if(EXISTS "${CURIO_PIXMAP}")
  install(FILES "${CURIO_PIXMAP}" DESTINATION share/pixmaps RENAME io.github.curio.Curio.png)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")
  install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png
          DESTINATION share/pixmaps
          RENAME io.github.curio.Curio.png)
endif()
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/resources/io.github.curio.Curio.metainfo.xml
        DESTINATION share/metainfo)

add_executable(curio-catalog-worker
    ${SRC_DIR}/tools/catalog_worker.cpp
    ${SRC_DIR}/backend/SandboxedFlatpakEnv.cpp
)
target_include_directories(curio-catalog-worker PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/models
    ${SRC_DIR}/backend
)
target_compile_definitions(curio-catalog-worker PRIVATE
    $<$<BOOL:${CURIO_SANDBOXED_LIBFLATPAK}>:CURIO_SANDBOXED_LIBFLATPAK=1>
)
target_link_libraries(curio-catalog-worker PRIVATE Qt6::Core)
install(TARGETS curio-catalog-worker RUNTIME DESTINATION bin)

