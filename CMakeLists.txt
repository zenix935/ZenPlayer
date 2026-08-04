cmake_minimum_required(VERSION 3.16)
project(ZenPlayer VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

qt_add_executable(ZenPlayer
    main.cpp
    ZenPlayer.h
    ZenPlayer.ui
    ZenPlayer.cpp
    ZenPlayer.qrc
    VlcEngine.h
    VlcEngine.cpp
    ClickableSlider.h
    equalizerDialog.h
    equalizerDialog.ui
    ListItemDelegate.h
    TrackItemDelegate.h
    createPlaylistDialog.h
    createPlaylistDialog.ui
    resource.rc
)

set_target_properties(ZenPlayer PROPERTIES
    WIN32_EXECUTABLE ON
)

target_link_libraries(ZenPlayer PRIVATE
    Qt6::Gui
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
)

if(APPLE)
    set_target_properties(ZenPlayer PROPERTIES
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.zenplayer.app"
        MACOSX_BUNDLE_BUNDLE_NAME "ZenPlayer"
        MACOSX_BUNDLE_BUNDLE_VERSION "2.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "2.0.0"
    )
endif()

if(UNIX AND NOT APPLE)
    install(TARGETS ZenPlayer DESTINATION bin)
    install(FILES pics/icon.ico DESTINATION share/pixmaps RENAME zenplayer.png)
    install(FILES zenplayer.desktop DESTINATION share/applications)
elseif(APPLE)
    install(TARGETS ZenPlayer BUNDLE DESTINATION .)
endif()

set(CPACK_PACKAGE_NAME "ZenPlayer")
set(CPACK_PACKAGE_VENDOR "ZenPlayer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Modern C++ Qt6 Music Player powered by libVLC")
set(CPACK_PACKAGE_VERSION "2.0.0")
set(CPACK_PACKAGE_CONTACT "smootharian@gmail.com")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "ZenPlayer v2.0.0")
    set(CPACK_SYSTEM_NAME "macOS")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "ZenPlayer Maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6 (>= 6.0), libqt6multimedia6 (>= 6.0), libvlc5 (>= 3.0)")

    set(CPACK_RPM_PACKAGE_NAME "ZenPlayer")
    set(CPACK_RPM_PACKAGE_SUMMARY "Modern C++ Qt6 Music Player powered by libVLC")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtmultimedia, vlc-core")
endif()

include(CPack)