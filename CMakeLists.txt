cmake_minimum_required(VERSION 3.16)
project(DiPDF VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# TÍNH NĂNG CHÍ MẠNG: Bật tự động tạo MOC cho Signal/Slot của Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Nạp các module cần thiết của Qt6
find_package(Qt6 COMPONENTS Widgets Network Qml Quick REQUIRED)

# Tìm thư viện Poppler-Qt6 và thiết lập dạng IMPORTED_TARGET để gộp link chuẩn
find_package(PkgConfig REQUIRED)
# ❌ bỏ Qt6 poppler
# pkg_check_modules(POPPLER_QT6 REQUIRED IMPORTED_TARGET poppler-qt6)

# ✅ dùng poppler-qt5 (có trên Ubuntu 22.04)
pkg_check_modules(POPPLER_QT5 REQUIRED IMPORTED_TARGET poppler-qt5)

# Danh sách file nguồn
set(PROJECT_SOURCES
    main.cpp
    mainwindow.h
    mainwindow.cpp
    theme_manager.h
    theme_manager.cpp
    pdfpagewidget.cpp
    resources.qrc
)

qt_add_executable(DiPDF ${PROJECT_SOURCES})

# Liên kết trực tiếp thông qua Target đã chuẩn hóa
target_link_libraries(DiPDF PRIVATE
        Qt6::Widgets
        Qt6::Network
        Qt6::Qml       # ✅ thêm
        Qt6::Quick     # ✅ thêm
        PkgConfig::POPPLER_QT5   # ✅ đổi sang QT5
)

qt_add_resources(RESOURCES resources.qrc)