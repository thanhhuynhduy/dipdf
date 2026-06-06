cmake_minimum_required(VERSION 3.16)
project(DiPDF VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TÍNH NĂNG CHÍ MẠNG: Bật tự động tạo MOC cho Signal/Slot của Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Nạp các module cần thiết của Qt6
find_package(Qt6 COMPONENTS Widgets Network Qml Quick Svg REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)

# Danh sách file nguồn
set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    theme_manager.h
    theme_manager.cpp
    pagecache.h
    pagecache.cpp
    pdfviewport.cpp
    resources.qrc
)

qt_add_executable(DiPDF ${PROJECT_SOURCES})

# Liên kết trực tiếp thông qua Target đã chuẩn hóa
target_link_libraries(DiPDF PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::Svg
        PkgConfig::POPPLER_QT6
)

target_include_directories(DiPDF PRIVATE
        /usr/include/qt6/QtCore5Compat
)

install(TARGETS DiPDF
        RUNTIME DESTINATION bin
)

install(FILES packaging/DiPDF.desktop
        DESTINATION share/applications
)

# Install all available icon sizes so desktop environments and AppImage
# integration can pick the best icon for the taskbar, Alt-Tab and launcher.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons/hicolor")
    install(DIRECTORY icons/hicolor
            DESTINATION share/icons
    )
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/DiPDF.png")
    install(FILES packaging/DiPDF.png
            DESTINATION share/icons/hicolor/256x256/apps
    )
endif()

# Keep the packaging icon too; linuxdeploy uses this file with --icon-file.
install(FILES packaging/DiPDF.png
        DESTINATION share/icons/hicolor/256x256/apps
)